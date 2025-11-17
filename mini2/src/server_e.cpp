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
#include "CommonUtils.hpp"
#include "SessionManager.hpp"

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

const std::string SERVER_E_PORT = "50055";
const int CHUNK_SIZE = 5;

std::string generate_uuid() {
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

class DataServiceImpl final : public DataService::Service {
private:
  AirQualityDataManager data_manager_;
  std::unique_ptr<SessionManager> session_manager_;

public:
  DataServiceImpl() : session_manager_(std::make_unique<SessionManager>()) {
    std::cout << "[Server E] Loading data from Sept 1-15 (20200901 to 20200915)..." << std::endl;
    std::string data_root = "../data/air_quality";

    for (int day = 1; day <= 15; day++) {
      std::stringstream folder_name;
      folder_name << "202009" << std::setfill('0') << std::setw(2) << day;
      std::string folder_path = data_root + "/" + folder_name.str();

      if (std::filesystem::exists(folder_path)) {
        data_manager_.loadFromDateFolder(folder_path);
      } else {
        std::cerr << "[Server E] Warning: Folder not found: " << folder_path << std::endl;
      }
    }

    std::cout << "[Server E] Initialized with " << data_manager_.getAllReadings().size()
              << " readings from Sept 1-15" << std::endl;
  }

  Status InitiateDataRequest(ServerContext *context, const Request *request, DataChunk *response) override {
    std::cout << "[Server E] Received InitiateDataRequest for query: " << request->name() << std::endl;

    std::string req_id = CommonUtils::generateRequestId("req_e");

    const auto &all_readings = data_manager_.getAllReadings();

    std::vector<AirQualityData> filtered_data;
    for (const auto &reading : all_readings) {
      filtered_data.push_back(CommonUtils::convertToProtobuf(reading));
    }

    std::cout << "[Server E] Prepared " << filtered_data.size() << " records for session " << req_id << std::endl;

    // Create session
    std::string session_id = session_manager_->createSession(filtered_data);

    // Get first chunk
    response->set_request_id(session_id);
    return session_manager_->getNextChunk(session_id, response);
  }

  Status GetNextChunk(ServerContext *context, const ChunkRequest *request, DataChunk *response) override {
    return session_manager_->getNextChunk(request->request_id(), response);
  }

  Status CancelRequest(ServerContext *context, const CancelRequestMessage *request, Ack *response) override {
    session_manager_->cancelSession(request->request_id());
    response->set_success(true);
    std::cout << "[Server E] Request " << request->request_id() << " cancelled successfully." << std::endl;
    return Status::OK;
  }
};

void RunServer() {
  DataServiceImpl service;
  std::string server_address("0.0.0.0:" + SERVER_E_PORT);

  ServerBuilder builder;
  
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server E (Pink Worker) listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char **argv) {
  RunServer();
  return 0;
}