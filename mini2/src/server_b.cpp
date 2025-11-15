#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>
#include "dataserver.grpc.pb.h"
#include "dataserver.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ClientContext;
using mini2::DataService;
using mini2::Request;
using mini2::DataChunk;
using mini2::ChunkRequest;
using mini2::CancelRequestMessage;
using mini2::Ack;

// Structure to track request mapping to Server C
struct RequestMapping {
  std::string worker_c_request_id;  // Server C's request ID
  std::chrono::steady_clock::time_point last_access;
};

// Server B - Green Team Leader (Streaming/Pass-Through)
class GreenTeamLeaderImpl final : public DataService::Service {
 public:
  GreenTeamLeaderImpl() {
    std::cout << "Server B: Initializing Green Team Leader..." << std::endl;

    // Connect to Worker C
    std::string worker_c_address = "localhost:50053";
    worker_c_stub_ = DataService::NewStub(
        grpc::CreateChannel(worker_c_address, grpc::InsecureChannelCredentials())
    );

    std::cout << "Server B: Connected to worker C at " << worker_c_address << std::endl;

    // Start cleanup thread for expired mappings
    running_ = true;
    cleanup_thread_ = std::thread(&GreenTeamLeaderImpl::cleanupExpiredMappings, this);
  }

  ~GreenTeamLeaderImpl() {
    running_ = false;
    if (cleanup_thread_.joinable()) {
      cleanup_thread_.join();
    }
  }

  // Forwards the initial data request to Server C, stores a mapping between
  Status InitiateDataRequest(ServerContext* context, const Request* request,
                         DataChunk* reply) override {
    std::cout << "Server B (Green Leader): Received request for: " 
              << request->name() << std::endl;

    // Generate our own request ID for tracking
    std::string our_request_id = generateRequestId();
    std::cout << "Server B: Generated request ID: " << our_request_id << std::endl;

    // Forward request to Worker C
    ClientContext worker_context;
    DataChunk worker_response;
    
    std::cout << "Server B: Forwarding request to worker C..." << std::endl;
    Status status = worker_c_stub_->InitiateDataRequest(&worker_context, *request, &worker_response);

    if (!status.ok()) {
      std::cerr << "Server B: ERROR - Worker C failed: " 
                << status.error_message() << std::endl;
      
      reply->set_request_id(our_request_id);
      reply->set_has_more_chunks(false);
      
      return Status(grpc::INTERNAL, "Failed to retrieve data from worker C");
    }

    // Store mapping: our_request_id → C's request_id
    std::string worker_request_id = worker_response.request_id();
    {
      std::lock_guard<std::mutex> lock(mappings_mutex_);
      request_mappings_[our_request_id] = {
          worker_request_id,
          std::chrono::steady_clock::now()
      };
    }

    std::cout << "Server B: Mapped request [" << our_request_id 
              << "] -> C's [" << worker_request_id << "]" << std::endl;
    std::cout << "Server B: Got first chunk with " 
              << worker_response.data_size() << " items from C" << std::endl;

    // Return first chunk to Server A with OUR request ID
    reply->CopyFrom(worker_response);
    reply->set_request_id(our_request_id);  // Replace C's ID with ours

    std::cout << "Server B: Returning first chunk with " << reply->data_size() 
              << " items to Server A" << std::endl;
    std::cout << "Server B: Has more chunks: " 
              << (reply->has_more_chunks() ? "Yes" : "No") << std::endl;

    return Status::OK;
  }

  // Forward GetNextChunk request to Server C using the stored request ID mapping
  Status GetNextChunk(ServerContext* context, const ChunkRequest* request,
                     DataChunk* reply) override {
    std::string our_request_id = request->request_id();
    std::cout << "Server B: Get next chunk for request ID: " << our_request_id << std::endl;

    // Look up C's request ID from our mapping
    std::string worker_request_id;
    {
      std::lock_guard<std::mutex> lock(mappings_mutex_);
      auto it = request_mappings_.find(our_request_id);
      
      if (it == request_mappings_.end()) {
        std::cerr << "Server B: ERROR - Request ID not found: " 
                  << our_request_id << std::endl;
        return Status(grpc::NOT_FOUND, "Request ID not found or expired");
      }
      
      worker_request_id = it->second.worker_c_request_id;
      // Update last access time
      it->second.last_access = std::chrono::steady_clock::now();
    }

    std::cout << "Server B: Forwarding to C's request ID: " << worker_request_id << std::endl;

    // Forward GetNextChunk to Server C
    ChunkRequest worker_chunk_req;
    worker_chunk_req.set_request_id(worker_request_id);
    
    ClientContext worker_context;
    DataChunk worker_response;
    
    Status status = worker_c_stub_->GetNextChunk(&worker_context, worker_chunk_req, &worker_response);
    
    if (!status.ok()) {
      std::cerr << "Server B: ERROR - Failed to get chunk from C: " 
                << status.error_message() << std::endl;
      return status;
    }

    std::cout << "Server B: Got chunk with " << worker_response.data_size() 
              << " items from C" << std::endl;

    // Return C's chunk to A (keeping our request ID)
    reply->CopyFrom(worker_response);
    reply->set_request_id(our_request_id);  // Replace C's ID with ours

    std::cout << "Server B: Forwarding chunk with " << reply->data_size() 
              << " items to Server A" << std::endl;
    std::cout << "Server B: Has more chunks: " 
              << (reply->has_more_chunks() ? "Yes" : "No") << std::endl;

    // Clean up mapping if no more chunks
    if (!reply->has_more_chunks()) {
      std::lock_guard<std::mutex> lock(mappings_mutex_);
      request_mappings_.erase(our_request_id);
      std::cout << "Server B: Request completed, removed mapping for: " 
                << our_request_id << std::endl;
    }

    return Status::OK;
  }

  Status CancelRequest(ServerContext* context, const CancelRequestMessage* request,
                    Ack* reply) override {
    std::string our_request_id = request->request_id();
    std::cout << "Server B: Cancel request ID: " << our_request_id << std::endl;
    
    // Look up C's request ID
    std::string worker_request_id;
    {
      std::lock_guard<std::mutex> lock(mappings_mutex_);
      auto it = request_mappings_.find(our_request_id);
      
      if (it != request_mappings_.end()) {
        worker_request_id = it->second.worker_c_request_id;
        request_mappings_.erase(it);
        std::cout << "Server B: Removed mapping for: " << our_request_id << std::endl;
      } else {
        std::cout << "Server B: Request ID not found (already expired?): " 
                  << our_request_id << std::endl;
        reply->set_success(true);
        return Status::OK;
      }
    }
    
    // Forward cancel to Server C
    std::cout << "Server B: Forwarding cancel to C's request ID: " 
              << worker_request_id << std::endl;
    
    CancelRequestMessage worker_cancel;
    worker_cancel.set_request_id(worker_request_id);
    
    ClientContext cancel_context;
    Ack cancel_ack;
    Status cancel_status = worker_c_stub_->CancelRequest(&cancel_context, worker_cancel, &cancel_ack);
    if (!cancel_status.ok()) {
      std::cerr << "Server B: WARNING - Failed to cancel request on Server C: " << cancel_status.error_message() << std::endl;
      std::cerr << "Server B: C's request ID was: " << worker_request_id << std::endl;
    }

    reply->set_success(true);
    std::cout << "Server B: Cancel completed" << std::endl;
    
    return Status::OK;
  }

 private:
  std::string generateRequestId() {
    static std::atomic<int> counter(0);
    return "req_b_green_" + std::to_string(++counter);
  }

  // Cleanup thread to remove expired mappings
  void cleanupExpiredMappings() {
    const auto EXPIRY_TIME = std::chrono::minutes(30);
    const auto CLEANUP_INTERVAL = std::chrono::minutes(5);  
    
    while (running_) {
      std::this_thread::sleep_for(CLEANUP_INTERVAL);
      
      auto now = std::chrono::steady_clock::now();
      std::lock_guard<std::mutex> lock(mappings_mutex_);
      
      for (auto it = request_mappings_.begin(); it != request_mappings_.end(); ) {
        if (now - it->second.last_access > EXPIRY_TIME) {
          std::cout << "Server B: Cleaning up expired mapping: " 
                    << it->first << std::endl;
          it = request_mappings_.erase(it);
        } else {
          ++it;
        }
      }
    }
  }

  std::unique_ptr<DataService::Stub> worker_c_stub_;
  std::mutex mappings_mutex_;
  std::unordered_map<std::string, RequestMapping> request_mappings_;
  
  std::atomic<bool> running_;
  std::thread cleanup_thread_;
};

void RunServerB() {
  std::string server_address("0.0.0.0:50052");
  GreenTeamLeaderImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  
  std::cout << "========================================" << std::endl;
  std::cout << "Server B (Green Team Leader) listening on " << server_address << std::endl;
  std::cout << "Managing Green team: B (self), C (worker)" << std::endl;
  std::cout << "Mode: STREAMING (pass-through chunking)" << std::endl;
  std::cout << "========================================" << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  std::cout << "Server B (Green Team Leader) starting..." << std::endl;
  RunServerB();
  return 0;
}