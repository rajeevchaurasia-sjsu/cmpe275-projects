#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <unordered_map>
#include <vector>
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

// Server B - Green Team Leader
// TODO: Implement coordination with C, handle Green team requests

class GreenTeamLeaderImpl final : public DataService::Service {
 public:
  GreenTeamLeaderImpl() {
    std::cout << "Server B: Initializing Green Team Leader..." << std::endl;

    // Connect to Worker C
    std::string worker_c_address = "localhost:50053";
    worker_c_stub_ = DataService::NewStub(
        grpc::CreateChannel(worker_c_address, grpc::InsecureChannelCredentials())
    );

    std::cout << "Server B: Connection stub created. Connected to worker C at " << worker_c_address << std::endl;
  }

  Status InitiateDataRequest(ServerContext* context, const Request* request,
                         DataChunk* reply) override {
    std::cout << "Server B (Green Leader): Received request for: " 
              << request->name() << std::endl;

    std::string request_id = generateRequestId();
    std::cout << "Server B: Generated request ID: " << request_id << std::endl;

    // Step 1: Get first chunk from worker C
    ClientContext worker_context;
    DataChunk worker_response;
    
    std::cout << "Server B: Forwarding request to worker C..." << std::endl;
    Status status = worker_c_stub_->InitiateDataRequest(&worker_context, *request, &worker_response);

    if (!status.ok()) {
      std::cerr << "Server B: ERROR - Worker C failed: " 
                << status.error_message() << std::endl;
      
      reply->set_request_id(request_id);
      reply->set_has_more_chunks(false);
      
      return Status(grpc::INTERNAL, "Failed to retrieve data from worker C");
    }

    // Step 2: Collect ALL chunks from C if chunked
    std::vector<mini2::AirQualityData> all_data;
    
    // Add first chunk data
    for (const auto& data : worker_response.data()) {
      all_data.push_back(data);
    }
    
    std::cout << "Server B: Got first chunk with " 
              << worker_response.data_size() << " items" << std::endl;

    // Step 3: Fetch remaining chunks if any
    std::string worker_request_id = worker_response.request_id();
    while (worker_response.has_more_chunks()) {
      ChunkRequest chunk_req;
      chunk_req.set_request_id(worker_request_id);
      
      ClientContext chunk_context;
      DataChunk next_chunk;
      
      Status chunk_status = worker_c_stub_->GetNextChunk(&chunk_context, chunk_req, &next_chunk);
      
      if (chunk_status.ok()) {
        std::cout << "Server B: Got next chunk with " 
                  << next_chunk.data_size() << " items" << std::endl;
        
        for (const auto& data : next_chunk.data()) {
          all_data.push_back(data);
        }
        
        worker_response.set_has_more_chunks(next_chunk.has_more_chunks());
      } else {
        std::cerr << "Server B: Warning - Failed to get chunk: " 
                  << chunk_status.error_message() << std::endl;
        break;
      }
    }

    std::cout << "Server B: Collected total of " << all_data.size() 
              << " items from worker C" << std::endl;

    // Step 4: Create our own response with ALL data
    DataChunk combined_chunk;
    combined_chunk.set_request_id(request_id);
    combined_chunk.set_has_more_chunks(false);  // We don't re-chunk
    
    for (const auto& data : all_data) {
      auto* data_item = combined_chunk.add_data();
      data_item->CopyFrom(data);
    }

    // Step 5: Store and return
    {
      std::lock_guard<std::mutex> lock(requests_mutex_);
      active_requests_[request_id] = combined_chunk;
    }

    reply->CopyFrom(combined_chunk);
    std::cout << "Server B: Returning " << reply->data_size() 
              << " items to Server A" << std::endl;

    return Status::OK;
  }

  Status GetNextChunk(ServerContext* context, const ChunkRequest* request,
                     DataChunk* reply) override {
    std::cout << "Server B: Get next chunk for request ID: " 
              << request->request_id() << std::endl;

    // Look up the original request in our cache
    std::lock_guard<std::mutex> lock(requests_mutex_);
    auto it = active_requests_.find(request->request_id());
    
    if (it != active_requests_.end()) {
      // Found it! Return the cached data
      reply->CopyFrom(it->second);
      reply->set_has_more_chunks(false);  // For now, no more chunks
      
      std::cout << "Server B: Returned chunk with " 
                << reply->data_size() << " items" << std::endl;
    } else {
      // Request ID not found - maybe it was cancelled?
      std::cerr << "Server B: ERROR - Request ID not found: " 
                << request->request_id() << std::endl;
      return Status(grpc::NOT_FOUND, "Request ID not found");
    }

    return Status::OK;
  }

  Status CancelRequest(ServerContext* context, const CancelRequestMessage* request,
                    Ack* reply) override {
    std::cout << "Server B: Cancel request ID: " << request->request_id() << std::endl;
    
    // Remove from cache
    {
      std::lock_guard<std::mutex> lock(requests_mutex_);
      active_requests_.erase(request->request_id());
    }
    
    // Tell Server C to cancel too
    ClientContext cancel_context;
    Ack cancel_ack;
    worker_c_stub_->CancelRequest(&cancel_context, *request, &cancel_ack);
    
    reply->set_success(true);
    return Status::OK;
  }

 private:

  std::string generateRequestId() {
    static int counter = 0;
    return "req_b_green_" + std::to_string(++counter);
  }

  std::unique_ptr<DataService::Stub> worker_c_stub_;
  std::mutex requests_mutex_;
  std::unordered_map<std::string, DataChunk> active_requests_;
};

void RunServerB() {
  std::string server_address("0.0.0.0:50052");
  GreenTeamLeaderImpl service;

  ServerBuilder builder;
  
  // Bind to port 50052
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  
  // Register our service implementation
  builder.RegisterService(&service);

  // Build and start the server
  std::unique_ptr<Server> server(builder.BuildAndStart());
  
  std::cout << "========================================" << std::endl;
  std::cout << "Server B (Green Team Leader) listening on " << server_address << std::endl;
  std::cout << "Managing Green team: B (self), C (worker)" << std::endl;
  std::cout << "========================================" << std::endl;

  // Wait for shutdown signal (Ctrl+C)
  server->Wait();
}

int main(int argc, char** argv) {
  std::cout << "Server B (Green Team Leader) starting..." << std::endl;
  RunServerB();
  return 0;
}