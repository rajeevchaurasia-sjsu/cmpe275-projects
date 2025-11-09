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

// Server A - Leader Server
// Routes client requests to team leaders B and D appropriately
class LeaderServiceImpl final : public DataService::Service {
 public:
  LeaderServiceImpl() {
    // Initialize connections to team leaders - B: Green team leader, D: Pink team leader
    team_b_leader_ = DataService::NewStub(
        grpc::CreateChannel("localhost:50052", grpc::InsecureChannelCredentials()));
    team_d_leader_ = DataService::NewStub(
        grpc::CreateChannel("localhost:50054", grpc::InsecureChannelCredentials()));
  }

  Status InitiateDataRequest(ServerContext* context, const Request* request,
                           DataChunk* reply) override {
    std::cout << "Server A: Received request for: " << request->name() << std::endl;

    // Generate unique request ID
    std::string request_id = generateRequestId();

    // Determine which teams to query based on request
    // For now, query both teams (Green and Pink)
    std::vector<std::string> teams_to_query = {"green", "pink"};

    // Start requests to both team leaders asynchronously
    std::vector<std::thread> team_threads;
    std::mutex results_mutex;
    std::vector<DataChunk> team_results;

    for (const auto& team : teams_to_query) {
      team_threads.emplace_back([&, team]() {
        DataChunk team_chunk;
        if (queryTeam(team, request, &team_chunk)) {
          std::lock_guard<std::mutex> lock(results_mutex);
          team_results.push_back(team_chunk);
        }
      });
    }

    // Wait for all team responses
    for (auto& thread : team_threads) {
      thread.join();
    }

    // Combine results from teams (for now, return first team's data)
    if (!team_results.empty()) {
      reply->CopyFrom(team_results[0]);
      reply->set_request_id(request_id);
      reply->set_has_more_chunks(true);  // Indicate more chunks available

      // Store request state for future chunk requests
      std::lock_guard<std::mutex> lock(requests_mutex_);
      active_requests_[request_id] = team_results[0];
    } else {
      reply->set_request_id(request_id);
      reply->set_has_more_chunks(false);
    }

    return Status::OK;
  }

  Status GetNextChunk(ServerContext* context, const ChunkRequest* request,
                     DataChunk* reply) override {
    std::cout << "Server A: Get next chunk for: " << request->request_id() << std::endl;

    std::lock_guard<std::mutex> lock(requests_mutex_);
    auto it = active_requests_.find(request->request_id());
    if (it != active_requests_.end()) {
      // For now, just return the same data (no actual chunking implemented yet)
      reply->CopyFrom(it->second);
      reply->set_has_more_chunks(false);  // No more chunks for now
    } else {
      return Status(grpc::NOT_FOUND, "Request ID not found");
    }

    return Status::OK;
  }

  Status CancelRequest(ServerContext* context, const CancelRequestMessage* request,
                      Ack* reply) override {
    std::cout << "Server A: Cancel request: " << request->request_id() << std::endl;

    std::lock_guard<std::mutex> lock(requests_mutex_);
    active_requests_.erase(request->request_id());

    reply->set_success(true);
    return Status::OK;
  }

 private:
  std::string generateRequestId() {
    static int counter = 0;
    return "req_a_" + std::to_string(++counter);
  }

  bool queryTeam(const std::string& team, const Request* request, DataChunk* chunk) {
    ClientContext context;
    Status status;

    if (team == "green") {
      status = team_b_stub_->InitiateDataRequest(&context, *request, chunk);
    } else if (team == "pink") {
      status = team_d_stub_->InitiateDataRequest(&context, *request, chunk);
    }

    if (status.ok()) {
      std::cout << "Server A: Got response from team " << team << std::endl;
      return true;
    } else {
      std::cout << "Server A: Failed to get response from team " << team
                << ": " << status.error_message() << std::endl;
      return false;
    }
  }

  std::unique_ptr<DataService::Stub> team_b_stub_;  // Green team leader
  std::unique_ptr<DataService::Stub> team_d_stub_;  // Pink team leader

  std::mutex requests_mutex_;
  std::unordered_map<std::string, DataChunk> active_requests_;
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

  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();
  return 0;
}