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
#include "CommonUtils.hpp"
#include <chrono>

using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using mini2::Ack;
using mini2::CancelRequestMessage;
using mini2::ChunkRequest;
using mini2::DataChunk;
using mini2::DataService;
using mini2::Request;

const std::string SERVER_B_ADDRESS = "169.254.119.126:50052";
const std::string SERVER_D_ADDRESS = "169.254.119.126:50054";
const std::string SERVER_A_PORT = "50051";

// Server A - Leader Server
// Routes client requests to appropriate team leaders (B and D)
class LeaderServiceImpl final : public DataService::Service
{
public:
  LeaderServiceImpl() : chunking_manager_(std::make_unique<ChunkingManager>()) {
    team_b_stub_ = DataService::NewStub(
        grpc::CreateChannel(SERVER_B_ADDRESS, grpc::InsecureChannelCredentials()));

    std::cout << "Server A: Connecting to Team B (Green) at " << SERVER_B_ADDRESS << std::endl;
    team_d_stub_ = DataService::NewStub(
        grpc::CreateChannel(SERVER_D_ADDRESS, grpc::InsecureChannelCredentials()));
    std::cout << "Server A: Connected to Team D (Pink) at " << SERVER_D_ADDRESS << std::endl;
  }

  Status InitiateDataRequest(ServerContext *context, const Request *request,
                             DataChunk *reply) override {

    auto total_start = std::chrono::high_resolution_clock::now();
    std::cout << "Server A: Received request for: " << request->name() << std::endl;

    std::string request_id = CommonUtils::generateRequestId("req_a");

    auto team_query_start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> team_threads;
    std::mutex results_mutex;
    std::vector<std::vector<mini2::AirQualityData>> team_results;

    std::vector<long long> team_times(2);

    team_threads.emplace_back([&, request]() {
      auto start = std::chrono::high_resolution_clock::now();
      std::vector<mini2::AirQualityData> data = queryTeam("green", request);
      auto end = std::chrono::high_resolution_clock::now();
      team_times[0] = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
      std::lock_guard<std::mutex> lock(results_mutex);
      team_results.push_back(data); 
    });

    team_threads.emplace_back([&, request]() {
      auto start = std::chrono::high_resolution_clock::now();
      std::vector<mini2::AirQualityData> data = queryTeam("pink", request);
      auto end = std::chrono::high_resolution_clock::now();
      team_times[1] = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
      std::lock_guard<std::mutex> lock(results_mutex);
      team_results.push_back(data); 
    });

    for (auto &thread : team_threads) {
      thread.join();
    }

    auto team_query_end = std::chrono::high_resolution_clock::now();
    long long team_query_duration = std::chrono::duration_cast<std::chrono::milliseconds>(team_query_end - team_query_start).count();

    std::vector<mini2::AirQualityData> combined_data;
    for (const auto &team_data : team_results) {
      combined_data.insert(combined_data.end(), team_data.begin(), team_data.end());
    }

    auto merge_end = std::chrono::high_resolution_clock::now();
    long long merge_duration = std::chrono::duration_cast<std::chrono::milliseconds>(merge_end - team_query_end).count();

    auto chunking_start = std::chrono::high_resolution_clock::now();

    const int CHUNK_SIZE = 10;
    std::deque<DataChunk> chunks;
    for (size_t i = 0; i < combined_data.size(); i += CHUNK_SIZE) {
      DataChunk chunk = CommonUtils::createChunk(combined_data, request_id, i, CHUNK_SIZE);
      chunks.push_back(chunk);
    }

    // Creating empty chunk (in case of no data)
    if (chunks.empty()) {
      DataChunk empty_chunk;
      empty_chunk.set_request_id(request_id);
      empty_chunk.set_has_more_chunks(false);
      chunks.push_back(empty_chunk);
    }

    chunking_manager_->storeChunks(request_id, chunks);
    *reply = chunks.front();

    auto chunking_end = std::chrono::high_resolution_clock::now();
    long long chunking_duration = std::chrono::duration_cast<std::chrono::milliseconds>(chunking_end - chunking_start).count();

    auto total_end = std::chrono::high_resolution_clock::now();
    long long total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

    std::cout << "\n========================================" << std::endl;
    std::cout << " SERVER A PERFORMANCE METRICS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Request ID: " << request_id << std::endl;
    std::cout << "Query: " << request->name() << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  Team Query Time: " << team_query_duration << "ms (parallel)" << std::endl;
    std::cout << "   └─ Green Team (B): " << team_times[0] << "ms" << std::endl;
    std::cout << "   └─ Pink Team (D): " << team_times[1] << "ms" << std::endl;
    std::cout << "  Data Merge Time: " << merge_duration << "ms" << std::endl;
    std::cout << "  Chunking Time: " << chunking_duration << "ms" << std::endl;
    std::cout << "  TOTAL TIME: " << total_duration << "ms" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Data Statistics:" << std::endl;
    std::cout << "   Total items: " << combined_data.size() << std::endl;
    std::cout << "   Total chunks: " << chunks.size() << std::endl;
    std::cout << "   Chunk size: " << CHUNK_SIZE << " items" << std::endl;
    std::cout << "   First chunk items: " << reply->data_size() << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << " Throughput:" << std::endl;
    if (total_duration > 0) {
      std::cout << "   " << (combined_data.size() * 1000.0 / total_duration) 
                << " items/sec" << std::endl;
      std::cout << "   " << (chunks.size() * 1000.0 / total_duration) 
                << " chunks/sec" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;    

    std::cout << "Server A: Created " << chunks.size() << " chunks with "
              << combined_data.size() << " total data items" << std::endl;

    return Status::OK;
  }

  Status GetNextChunk(ServerContext *context, const ChunkRequest *request,
                      DataChunk *reply) override {
    std::cout << "Server A: Get next chunk for: " << request->request_id() << std::endl;

    return chunking_manager_->getNextChunk(request->request_id(), reply);
  }

  Status CancelRequest(ServerContext *context, const CancelRequestMessage *request,
                       Ack *reply) override {
    std::cout << "Server A: Cancel request: " << request->request_id() << std::endl;

    chunking_manager_->cancelRequest(request->request_id());

    reply->set_success(true);
    return Status::OK;
  }

private:
  std::vector<mini2::AirQualityData> queryTeam(const std::string &team, const Request *request) {
    std::vector<mini2::AirQualityData> result;

    try {
      ClientContext context;
      DataChunk response;

      Status status;
      if (team == "green") {
        std::cout << "Server A: Querying team " << team << "..." << std::endl;
        status = team_b_stub_->InitiateDataRequest(&context, *request, &response);
      } else if (team == "pink") {
        std::cout << "Server A: Querying team " << team << "..." << std::endl;
        status = team_d_stub_->InitiateDataRequest(&context, *request, &response);
      } else {
        std::cout << "Server A: Unknown team: " << team << std::endl;
        return result;
      }

      if (status.ok()) {
        std::cout << "Server A: Got " << response.data_size() << " items from team " << team << std::endl;
        for (const auto &data : response.data()) {
          result.push_back(data);
        }

        while (response.has_more_chunks()) {
          ChunkRequest chunk_req;
          chunk_req.set_request_id(response.request_id());

          ClientContext chunk_context;
          DataChunk next_chunk;
          Status chunk_status;
          if (team == "green") {
            chunk_status = team_b_stub_->GetNextChunk(&chunk_context, chunk_req, &next_chunk);
          } else if (team == "pink") {
            chunk_status = team_d_stub_->GetNextChunk(&chunk_context, chunk_req, &next_chunk);
          } else {
            break;
          }

          if (chunk_status.ok()) {
            std::cout << "Server A: Got chunk with " << next_chunk.data_size() << " items from team " << team << std::endl;
            for (const auto &data : next_chunk.data()) {
              result.push_back(data);
            }
            response.set_has_more_chunks(next_chunk.has_more_chunks());
          } else {
            std::cerr << "Server A: Failed to get next chunk from team " << team << std::endl;
            break;
          }
        }

        std::cout << "Server A: Total collected " << result.size() << " items from team " << team << std::endl;
      } else {
        std::cerr << "Server A: Failed to query team " << team
                  << ": " << status.error_message() << std::endl;
      }
    } catch (const std::exception &e) {
      std::cerr << "Server A: Exception querying team " << team << ": " << e.what() << std::endl;
    }

    return result;
  }

private:
  std::unique_ptr<DataService::Stub> team_b_stub_;
  std::unique_ptr<DataService::Stub> team_d_stub_;

  std::unique_ptr<ChunkingManager> chunking_manager_;
};

void RunServer() {
  std::string server_address("0.0.0.0:" + SERVER_A_PORT);
  LeaderServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());

  std::cout << "========================================" << std::endl;
  std::cout << "Server A (Leader) listening on " << server_address << std::endl;
  std::cout << "Connected to Team B (Green): " << SERVER_B_ADDRESS << std::endl;
  std::cout << "Connected to Team D (Pink): " << SERVER_D_ADDRESS << std::endl;
  std::cout << "Testing with GREEN TEAM ONLY (B, C)" << std::endl;
  std::cout << "Chunking enabled - 10 items per chunk" << std::endl;
  std::cout << "========================================" << std::endl;

  server->Wait();
}

int main(int argc, char **argv) {
  std::cout << "Server A (Leader) starting..." << std::endl;
  RunServer();
  return 0;
}