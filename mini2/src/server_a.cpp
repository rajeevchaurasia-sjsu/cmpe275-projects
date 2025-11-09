#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <thread>
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

#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <deque>
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

// Structure to track chunked request state
struct ChunkedRequest {
  std::deque<DataChunk> chunks;
  int current_chunk_index = 0;
  std::chrono::steady_clock::time_point last_access;

  ChunkedRequest() : last_access(std::chrono::steady_clock::now()) {}
};

// Server A - Leader Server
// Routes client requests to appropriate team leaders (B and D)
class LeaderServiceImpl final : public DataService::Service {
 public:
  LeaderServiceImpl() {
    // Initialize connections to team leaders
    // B: Green team leader, D: Pink team leader
    team_b_stub_ = DataService::NewStub(
        grpc::CreateChannel("localhost:50052", grpc::InsecureChannelCredentials()));
    team_d_stub_ = DataService::NewStub(
        grpc::CreateChannel("localhost:50054", grpc::InsecureChannelCredentials()));

    // Start cleanup thread for expired requests
    cleanup_thread_ = std::thread(&LeaderServiceImpl::cleanupExpiredRequests, this);
  }

  ~LeaderServiceImpl() {
    running_ = false;
    if (cleanup_thread_.joinable()) {
      cleanup_thread_.join();
    }
  }

  Status InitiateDataRequest(ServerContext* context, const Request* request,
                           DataChunk* reply) override {
    std::cout << "Server A: Received request for: " << request->name() << std::endl;

    // Generate unique request ID
    std::string request_id = generateRequestId();

    // Query both teams asynchronously
    std::vector<std::thread> team_threads;
    std::mutex results_mutex;
    std::vector<std::vector<mini2::AirQualityData>> team_results;

    team_threads.emplace_back([&, request]() {
      std::vector<mini2::AirQualityData> data = queryTeam("green", request);
      std::lock_guard<std::mutex> lock(results_mutex);
      team_results.push_back(data);
    });

    team_threads.emplace_back([&, request]() {
      std::vector<mini2::AirQualityData> data = queryTeam("pink", request);
      std::lock_guard<std::mutex> lock(results_mutex);
      team_results.push_back(data);
    });

    // Wait for all team responses
    for (auto& thread : team_threads) {
      thread.join();
    }

    // Combine and chunk the data
    std::vector<mini2::AirQualityData> combined_data;
    for (const auto& team_data : team_results) {
      combined_data.insert(combined_data.end(), team_data.begin(), team_data.end());
    }

    // Create chunks (e.g., 10 items per chunk)
    const int CHUNK_SIZE = 10;
    std::deque<DataChunk> chunks;
    for (size_t i = 0; i < combined_data.size(); i += CHUNK_SIZE) {
      DataChunk chunk;
      chunk.set_request_id(request_id);
      size_t end = std::min(i + CHUNK_SIZE, combined_data.size());
      for (size_t j = i; j < end; ++j) {
        auto* data_item = chunk.add_data();
        data_item->CopyFrom(combined_data[j]);
      }
      chunk.set_has_more_chunks((end < combined_data.size()));
      chunks.push_back(chunk);
    }

    // If no data, create empty chunk
    if (chunks.empty()) {
      DataChunk empty_chunk;
      empty_chunk.set_request_id(request_id);
      empty_chunk.set_has_more_chunks(false);
      chunks.push_back(empty_chunk);
    }

    // Store chunked request state
    {
      std::lock_guard<std::mutex> lock(requests_mutex_);
      ChunkedRequest req_state;
      req_state.chunks = chunks;
      req_state.current_chunk_index = 0;
      chunked_requests_[request_id] = req_state;
    }

    // Return first chunk
    reply->CopyFrom(chunks.front());

    std::cout << "Server A: Created " << chunks.size() << " chunks with "
              << combined_data.size() << " total data items" << std::endl;

    return Status::OK;
  }

  Status GetNextChunk(ServerContext* context, const ChunkRequest* request,
                     DataChunk* reply) override {
    std::cout << "Server A: Get next chunk for: " << request->request_id() << std::endl;

    std::lock_guard<std::mutex> lock(requests_mutex_);
    auto it = chunked_requests_.find(request->request_id());
    if (it == chunked_requests_.end()) {
      return Status(grpc::NOT_FOUND, "Request ID not found");
    }

    ChunkedRequest& req_state = it->second;
    req_state.last_access = std::chrono::steady_clock::now();

    if (req_state.current_chunk_index + 1 >= static_cast<int>(req_state.chunks.size())) {
      return Status(grpc::OUT_OF_RANGE, "No more chunks available");
    }

    req_state.current_chunk_index++;
    reply->CopyFrom(req_state.chunks[req_state.current_chunk_index]);

    std::cout << "Server A: Returning chunk " << req_state.current_chunk_index
              << " of " << req_state.chunks.size() << std::endl;

    return Status::OK;
  }

  Status CancelRequest(ServerContext* context, const CancelRequestMessage* request,
                      Ack* reply) override {
    std::cout << "Server A: Cancel request: " << request->request_id() << std::endl;

    std::lock_guard<std::mutex> lock(requests_mutex_);
    chunked_requests_.erase(request->request_id());

    reply->set_success(true);
    return Status::OK;
  }

 private:
  std::string generateRequestId() {
    static int counter = 0;
    return "req_a_" + std::to_string(++counter);
  }

  std::vector<mini2::AirQualityData> queryTeam(const std::string& team, const Request* request) {
    std::vector<mini2::AirQualityData> result;

    try {
      ClientContext context;
      DataChunk response;

      Status status;
      if (team == "green") {
        status = team_b_stub_->InitiateDataRequest(&context, *request, &response);
      } else if (team == "pink") {
        status = team_d_stub_->InitiateDataRequest(&context, *request, &response);
      }

      if (status.ok()) {
        std::cout << "Server A: Got " << response.data_size() << " items from team " << team << std::endl;
        for (const auto& data : response.data()) {
          result.push_back(data);
        }

        // If team returns chunked data, collect all chunks
        while (response.has_more_chunks()) {
          ChunkRequest chunk_req;
          chunk_req.set_request_id(response.request_id());

          ClientContext chunk_context;
          DataChunk next_chunk;
          Status chunk_status;
          if (team == "green") {
            chunk_status = team_b_stub_->GetNextChunk(&chunk_context, chunk_req, &next_chunk);
          } else {
            chunk_status = team_d_stub_->GetNextChunk(&chunk_context, chunk_req, &next_chunk);
          }

          if (chunk_status.ok()) {
            for (const auto& data : next_chunk.data()) {
              result.push_back(data);
            }
            response.set_has_more_chunks(next_chunk.has_more_chunks());
          } else {
            break;
          }
        }
      } else {
        std::cout << "Server A: Failed to query team " << team
                  << ": " << status.error_message() << std::endl;
      }
    } catch (const std::exception& e) {
      std::cout << "Server A: Exception querying team " << team << ": " << e.what() << std::endl;
    }

    return result;
  }

  void cleanupExpiredRequests() {
    while (running_) {
      std::this_thread::sleep_for(std::chrono::minutes(5)); // Cleanup every 5 minutes

      std::lock_guard<std::mutex> lock(requests_mutex_);
      auto now = std::chrono::steady_clock::now();
      auto timeout = std::chrono::minutes(30); // 30 minute timeout

      for (auto it = chunked_requests_.begin(); it != chunked_requests_.end(); ) {
        if (now - it->second.last_access > timeout) {
          it = chunked_requests_.erase(it);
        } else {
          ++it;
        }
      }
    }
  }

  std::unique_ptr<DataService::Stub> team_b_stub_;  // Green team leader
  std::unique_ptr<DataService::Stub> team_d_stub_;  // Pink team leader

  std::mutex requests_mutex_;
  std::unordered_map<std::string, ChunkedRequest> chunked_requests_;

  std::thread cleanup_thread_;
  std::atomic<bool> running_{true};
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  LeaderServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server A (Leader) listening on " << server_address << std::endl;
  std::cout << "Connected to Team B (Green): localhost:50052" << std::endl;
  std::cout << "Connected to Team D (Pink): localhost:50054" << std::endl;
  std::cout << "Chunking enabled - supports large dataset segmentation" << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();
  return 0;
}