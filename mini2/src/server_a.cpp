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

// Server A - Leader Server
// Routes client requests to appropriate team leaders (B and D)
class LeaderServiceImpl final : public DataService::Service
{
public:
  LeaderServiceImpl() : chunking_manager_(std::make_unique<ChunkingManager>())
  {
    // Initialize connections to team leaders
    // B: Green team leader, D: Pink team leader
    team_b_stub_ = DataService::NewStub(
        grpc::CreateChannel("169.254.156.148:50052", grpc::InsecureChannelCredentials()));

    std::cout << "Server A: Connecting to Team B (Green) at 169.254.156.148:50052" << std::endl;
    team_d_stub_ = DataService::NewStub(
        grpc::CreateChannel("169.254.119.126:50054", grpc::InsecureChannelCredentials()));
    std::cout << "Server A: Connected to Team D (Pink): 169.254.119.126:50054" << std::endl;
  }

  Status InitiateDataRequest(ServerContext *context, const Request *request,
                             DataChunk *reply) override
  {
    std::cout << "Server A: Received request for: " << request->name() << std::endl;

    // Generate unique request ID
    std::string request_id = CommonUtils::generateRequestId("req_a");

    // Query both teams asynchronously
    std::vector<std::thread> team_threads;
    std::mutex results_mutex;
    std::vector<std::vector<mini2::AirQualityData>> team_results;

    // Query Green team
    team_threads.emplace_back([&, request]()
                              {
      std::vector<mini2::AirQualityData> data = queryTeam("green", request);
      std::lock_guard<std::mutex> lock(results_mutex);
      team_results.push_back(data); });

    team_threads.emplace_back([&, request]()
                              {
      std::vector<mini2::AirQualityData> data = queryTeam("pink", request);
      std::lock_guard<std::mutex> lock(results_mutex);
      team_results.push_back(data); });

    // Wait for all team responses
    for (auto &thread : team_threads)
    {
      thread.join();
    }

    // Combine and chunk the data
    std::vector<mini2::AirQualityData> combined_data;
    for (const auto &team_data : team_results)
    {
      combined_data.insert(combined_data.end(), team_data.begin(), team_data.end());
    }

    // Create chunks using ChunkingManager
    const int CHUNK_SIZE = 10;
    std::deque<DataChunk> chunks;
    for (size_t i = 0; i < combined_data.size(); i += CHUNK_SIZE)
    {
      DataChunk chunk = CommonUtils::createChunk(combined_data, request_id, i, CHUNK_SIZE);
      chunks.push_back(chunk);
    }

    // If no data, create empty chunk
    if (chunks.empty())
    {
      DataChunk empty_chunk;
      empty_chunk.set_request_id(request_id);
      empty_chunk.set_has_more_chunks(false);
      chunks.push_back(empty_chunk);
    }

    // Store chunked request state using ChunkingManager
    chunking_manager_->storeChunks(request_id, chunks);

    // Return first chunk
    *reply = chunks.front();

    std::cout << "Server A: Created " << chunks.size() << " chunks with "
              << combined_data.size() << " total data items" << std::endl;

    return Status::OK;
  }

  Status GetNextChunk(ServerContext *context, const ChunkRequest *request,
                      DataChunk *reply) override
  {
    std::cout << "Server A: Get next chunk for: " << request->request_id() << std::endl;

    return chunking_manager_->getNextChunk(request->request_id(), reply);
  }

  Status CancelRequest(ServerContext *context, const CancelRequestMessage *request,
                       Ack *reply) override
  {
    std::cout << "Server A: Cancel request: " << request->request_id() << std::endl;

    chunking_manager_->cancelRequest(request->request_id());

    reply->set_success(true);
    return Status::OK;
  }

private:
  std::vector<mini2::AirQualityData> queryTeam(const std::string &team, const Request *request)
  {
    std::vector<mini2::AirQualityData> result;

    try
    {
      ClientContext context;
      DataChunk response;

      Status status;
      if (team == "green")
      {
        std::cout << "Server A: Querying team " << team << "..." << std::endl;
        status = team_b_stub_->InitiateDataRequest(&context, *request, &response);
      }
      else if (team == "pink")
      {
        std::cout << "Server A: Querying team " << team << "..." << std::endl;
        status = team_d_stub_->InitiateDataRequest(&context, *request, &response);
      }
      else
      {
        std::cout << "Server A: Unknown team: " << team << std::endl;
        return result;
      }

      if (status.ok())
      {
        std::cout << "Server A: Got " << response.data_size() << " items from team " << team << std::endl;
        for (const auto &data : response.data())
        {
          result.push_back(data);
        }

        // If team returns chunked data, collect all chunks
        while (response.has_more_chunks())
        {
          ChunkRequest chunk_req;
          chunk_req.set_request_id(response.request_id());

          ClientContext chunk_context;
          DataChunk next_chunk;
          Status chunk_status;
          if (team == "green")
          {
            chunk_status = team_b_stub_->GetNextChunk(&chunk_context, chunk_req, &next_chunk);
          }
          else if (team == "pink")
          {
            chunk_status = team_d_stub_->GetNextChunk(&chunk_context, chunk_req, &next_chunk);
          }
          else
          {
            break;
          }

          if (chunk_status.ok())
          {
            std::cout << "Server A: Got chunk with " << next_chunk.data_size() << " items from team " << team << std::endl;
            for (const auto &data : next_chunk.data())
            {
              result.push_back(data);
            }
            response.set_has_more_chunks(next_chunk.has_more_chunks());
          }
          else
          {
            std::cerr << "Server A: Failed to get next chunk from team " << team << std::endl;
            break;
          }
        }

        std::cout << "Server A: Total collected " << result.size() << " items from team " << team << std::endl;
      }
      else
      {
        std::cerr << "Server A: Failed to query team " << team
                  << ": " << status.error_message() << std::endl;
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Server A: Exception querying team " << team << ": " << e.what() << std::endl;
    }

    return result;
  }

private:
  std::unique_ptr<DataService::Stub> team_b_stub_; // Green team leader
  std::unique_ptr<DataService::Stub> team_d_stub_; // Pink team leader

  std::unique_ptr<ChunkingManager> chunking_manager_;
};

void RunServer()
{
  std::string server_address("0.0.0.0:50051");
  LeaderServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());

  std::cout << "========================================" << std::endl;
  std::cout << "Server A (Leader) listening on " << server_address << std::endl;
  std::cout << "Connected to Team B (Green): 192.168.156.148:50052" << std::endl;
  std::cout << "Connected to Team D (Pink): 169.254.119.126:50054" << std::endl;
  std::cout << "Testing with GREEN TEAM ONLY (B, C)" << std::endl;
  std::cout << "Chunking enabled - 10 items per chunk" << std::endl;
  std::cout << "========================================" << std::endl;

  server->Wait();
}

int main(int argc, char **argv)
{
  std::cout << "Server A (Leader) starting..." << std::endl;
  RunServer();
  return 0;
}