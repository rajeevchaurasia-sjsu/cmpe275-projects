#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include "dataserver.grpc.pb.h"
#include "dataserver.pb.h"
#include "../include/AirQualityDataManager.hpp"
#include "../include/CommonUtils.hpp"
#include "../include/SessionManager.hpp"
#include "../utils/CSVParser.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using mini2::DataService;
using mini2::Request;
using mini2::DataChunk;
using mini2::ChunkRequest;
using mini2::CancelRequestMessage;
using mini2::Ack;
using mini2::AirQualityData;

const std::string SERVER_C_PORT = "50053";

// Server C - Green Team Worker
// Processes data requests for the Green team with chunking support
class WorkerServiceImpl final : public DataService::Service {
 public:
  WorkerServiceImpl() {
    // Initialize real air quality data from Mini 1
    initializeRealData();

    // Initialize session manager for chunking (5 items per chunk)
    session_manager_ = std::make_unique<SessionManager>(5);
  }

  Status InitiateDataRequest(ServerContext* context, const Request* request,
                           DataChunk* reply) override {
    std::cout << "Server C (Green Worker): Processing request for: " << request->name() << std::endl;

    // Get data for this request (Green team data)
    auto all_data = getGreenTeamData(request->name());

    // Create session and get first chunk
    std::string session_id = session_manager_->createSession(all_data);
    return session_manager_->getNextChunk(session_id, reply);
  }

  Status GetNextChunk(ServerContext* context, const ChunkRequest* request,
                     DataChunk* reply) override {
    std::cout << "Server C: Get next chunk for: " << request->request_id() << std::endl;
    return session_manager_->getNextChunk(request->request_id(), reply);
  }

  Status CancelRequest(ServerContext* context, const CancelRequestMessage* request,
                      Ack* reply) override {
    std::cout << "Server C: Cancel request: " << request->request_id() << std::endl;
    session_manager_->cancelSession(request->request_id());
    reply->set_success(true);
    return Status::OK;
  }

 private:
  void initializeRealData() {
    // Load real air quality data from Mini 2 data directory
    // Server C serves GREEN TEAM data: August 2020 only (20200801-20200831)
    std::string data_root = "../data/air_quality";

    std::cout << "Server C: Loading GREEN TEAM data from August 2020 (20200801-20200831)..." << std::endl;

    // Load specific date folders for Server C (all days of August)
    for (int day = 1; day <= 31; day++) {
      std::stringstream folder_name;
      folder_name << "202008" << std::setfill('0') << std::setw(2) << day;
      std::string folder_path = data_root + "/" + folder_name.str();

      // Check if folder exists and load it
      if (std::filesystem::exists(folder_path)) {
        dataManager_.loadFromDateFolder(folder_path);
      } else {
        std::cerr << "Server C: Warning: Folder not found: " << folder_path << std::endl;
      }
    }

    int totalReadings = dataManager_.getReadingCount();
    std::cout << "Server C: Loaded " << totalReadings << " air quality readings from August 2020" << std::endl;

    // Convert to protobuf format and store
    auto allReadings = dataManager_.getAllReadings();
    for (const auto& reading : allReadings) {
      green_team_data_["real"].push_back(CommonUtils::convertToProtobuf(reading));
    }

    std::cout << "Server C: Converted " << green_team_data_["real"].size()
              << " readings to protobuf format" << std::endl;
  }

  void initializeSampleData() {
    // Fallback sample data in case real data loading fails
    for (int i = 1; i <= 25; ++i) {
      mini2::AirQualityData data;
      data.set_datetime("2020-08-10T" + std::to_string(10 + i) + ":00:00Z");
      data.set_timezone("UTC");
      data.set_location("Green City " + std::to_string(i));
      data.set_latitude(37.7749 + i * 0.01);
      data.set_longitude(-122.4194 + i * 0.01);
      data.set_aqi_parameter(i % 2 == 0 ? "PM2.5" : "PM10");
      data.set_aqi_value(20.0 + i * 2.5);
      data.set_aqi_unit("µg/m³");
      data.set_aqi_category(i % 3 == 0 ? "Good" : (i % 3 == 1 ? "Moderate" : "Unhealthy"));

      green_team_data_["sample"].push_back(data);
    }

    std::cout << "Server C: Initialized with " << green_team_data_["sample"].size()
              << " sample data items (fallback)" << std::endl;
  }

  std::vector<mini2::AirQualityData> getGreenTeamData(const std::string& query) {
    // Try to return real data first, fallback to sample data
    auto it = green_team_data_.find("real");
    if (it != green_team_data_.end() && !it->second.empty()) {
      return it->second;
    }

    // Fallback to sample data
    it = green_team_data_.find("sample");
    if (it != green_team_data_.end()) {
      return it->second;
    }

    return {};
  }

  std::unordered_map<std::string, std::vector<mini2::AirQualityData>> green_team_data_;
  std::unique_ptr<SessionManager> session_manager_;
  AirQualityDataManager dataManager_;
};

void RunServer() {
  std::string server_address("0.0.0.0:" + SERVER_C_PORT);
  WorkerServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server C (Green Worker) listening on " << server_address << std::endl;
  std::cout << "Serving Green team data with chunking support" << std::endl;
  std::cout << "Chunk size: 5 items per chunk" << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();
  return 0;
}