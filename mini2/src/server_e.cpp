/*
 * server_e.cpp
 * Pink Team Worker - C++ Implementation
 * * Roles:
 * - Serves a subset of Air Quality Data
 * - Handles Chunked Data Transfer
 * - Manages request sessions
 */

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>
#include <vector>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <filesystem>

#include <grpcpp/grpcpp.h>
#include "dataserver.grpc.pb.h"
#include "AirQualityDataManager.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using mini2::Ack;
using mini2::AirQualityData;
using mini2::CancelRequestMessage;
using mini2::ChunkRequest;
using mini2::DataChunk;
using mini2::DataService;
using mini2::Request;

// Define the port for Server E (Pink Worker)
// Assuming A=50051, B=50052, C=50053, D=50054, E=50055
const std::string SERVER_ADDRESS = "0.0.0.0:50055";
const int CHUNK_SIZE = 5; // Number of records per chunk (keep small to demo streaming)

// helper to generate UUIDs for session management
std::string generate_uuid()
{
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 15);
  static std::uniform_int_distribution<> dis2(8, 11);

  std::stringstream ss;
  int i;
  ss << std::hex;
  for (i = 0; i < 8; i++)
    ss << dis(gen);
  ss << "-";
  for (i = 0; i < 4; i++)
    ss << dis(gen);
  ss << "-4";
  for (i = 0; i < 3; i++)
    ss << dis(gen);
  ss << "-";
  ss << dis2(gen);
  for (i = 0; i < 3; i++)
    ss << dis(gen);
  ss << "-";
  for (i = 0; i < 12; i++)
    ss << dis(gen);
  return ss.str();
}

// Implementation of the DataService
class DataServiceImpl final : public DataService::Service
{
private:
  // Struct to track the state of a client's request
  struct Session
  {
    std::vector<AirQualityData> filtered_data;
    size_t current_index;
  };

  // In-memory session store: RequestID -> Session
  std::map<std::string, Session> sessions_;
  std::mutex mutex_;

  // Reference to the data manager
  AirQualityDataManager data_manager_;

public:
  DataServiceImpl()
  {
    // Load data from Sept 1-15 (folders 20200901 to 20200915)
    std::cout << "[Server E] Loading data from Sept 1-15 (20200901 to 20200915)..." << std::endl;

    std::string data_root = "../data/air_quality";

    // Load specific date folders for Server E (first 15 days of September)
    for (int day = 1; day <= 15; day++)
    {
      std::stringstream folder_name;
      folder_name << "202009" << std::setfill('0') << std::setw(2) << day;
      std::string folder_path = data_root + "/" + folder_name.str();

      // Check if folder exists and load it
      if (std::filesystem::exists(folder_path))
      {
        data_manager_.loadFromDateFolder(folder_path);
      }
      else
      {
        std::cerr << "[Server E] Warning: Folder not found: " << folder_path << std::endl;
      }
    }

    std::cout << "[Server E] Initialized with " << data_manager_.getAllReadings().size()
              << " readings from Sept 1-15" << std::endl;
  }

  // 1. Initiate a Request
  Status InitiateDataRequest(ServerContext *context, const Request *request, DataChunk *response) override
  {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[Server E] Received InitiateDataRequest for query: " << request->name() << std::endl;

    // Create a new session
    std::string req_id = generate_uuid();
    Session session;
    session.current_index = 0;

    // Server E serves ALL data from Sept 1-15
    // In a real app, this would filter based on request->name()
    const auto &all_data = data_manager_.getAllReadings();

    // Copy converting generic Reading object to Proto object
    for (const auto &reading : all_data)
    {
      AirQualityData proto_data;
      proto_data.set_datetime(reading.getDatetime());
      proto_data.set_location(reading.getSiteName());
      proto_data.set_aqi_value(reading.getValue());
      proto_data.set_aqi_parameter(reading.getPollutantType());
      session.filtered_data.push_back(proto_data);
    }

    std::cout << "[Server E] Prepared " << session.filtered_data.size() << " records for session " << req_id << std::endl;

    // Save session
    sessions_[req_id] = session;

    // Prepare the first chunk
    response->set_request_id(req_id);

    // Logic to fill the first chunk is same as GetNextChunk, so we just call internal helper or logic here
    size_t items_to_send = std::min((size_t)CHUNK_SIZE, session.filtered_data.size());

    for (size_t i = 0; i < items_to_send; ++i)
    {
      *response->add_data() = session.filtered_data[i];
    }

    // Update Index
    sessions_[req_id].current_index = items_to_send;

    // Check if more
    response->set_has_more_chunks(sessions_[req_id].current_index < session.filtered_data.size());

    return Status::OK;
  }

  // 2. Get Next Chunk
  Status GetNextChunk(ServerContext *context, const ChunkRequest *request, DataChunk *response) override
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string req_id = request->request_id();

    // Check if session exists
    if (sessions_.find(req_id) == sessions_.end())
    {
      return Status(grpc::StatusCode::NOT_FOUND, "Session not found or expired");
    }

    Session &session = sessions_[req_id];

    // Calculate range
    size_t start = session.current_index;
    size_t remaining = session.filtered_data.size() - start;
    size_t items_to_send = std::min((size_t)CHUNK_SIZE, remaining);

    // Fill Response
    response->set_request_id(req_id);
    for (size_t i = 0; i < items_to_send; ++i)
    {
      *response->add_data() = session.filtered_data[start + i];
    }

    // Update state
    session.current_index += items_to_send;
    response->set_has_more_chunks(session.current_index < session.filtered_data.size());

    std::cout << "[Server E] Sent chunk for " << req_id << ". Progress: "
              << session.current_index << "/" << session.filtered_data.size() << std::endl;

    // Cleanup if done
    if (session.current_index >= session.filtered_data.size())
    {
      std::cout << "[Server E] Transfer complete. Removing session " << req_id << std::endl;
      sessions_.erase(req_id);
    }

    return Status::OK;
  }

  // 3. Cancel Request
  Status CancelRequest(ServerContext *context, const CancelRequestMessage *request, Ack *response) override
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string req_id = request->request_id();

    if (sessions_.find(req_id) != sessions_.end())
    {
      sessions_.erase(req_id);
      response->set_success(true);
      std::cout << "[Server E] Request " << req_id << " cancelled successfully." << std::endl;
    }
    else
    {
      response->set_success(false);
      std::cout << "[Server E] Cancel failed. Session " << req_id << " not found." << std::endl;
    }

    return Status::OK;
  }
};

void RunServer()
{
  DataServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(SERVER_ADDRESS, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);

  // Assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server E (Pink Worker) listening on " << SERVER_ADDRESS << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char **argv)
{
  RunServer();
  return 0;
}