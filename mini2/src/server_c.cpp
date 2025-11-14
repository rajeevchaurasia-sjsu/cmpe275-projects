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

// Structure to track chunked request state
struct ChunkedRequest {
  std::deque<DataChunk> chunks;
  int current_chunk_index = 0;
  std::chrono::steady_clock::time_point last_access;

  ChunkedRequest() : last_access(std::chrono::steady_clock::now()) {}
};

// Server C - Green Team Worker
// Processes data requests for the Green team with chunking support
class WorkerServiceImpl final : public DataService::Service {
 public:
  WorkerServiceImpl() {
    // Initialize real air quality data from Mini 1
    initializeRealData();

    // Start cleanup thread for expired requests
    cleanup_thread_ = std::thread(&WorkerServiceImpl::cleanupExpiredRequests, this);
  }

  ~WorkerServiceImpl() {
    running_ = false;
    if (cleanup_thread_.joinable()) {
      cleanup_thread_.join();
    }
  }

  Status InitiateDataRequest(ServerContext* context, const Request* request,
                           DataChunk* reply) override {
    std::cout << "Server C (Green Worker): Processing request for: " << request->name() << std::endl;

    // Generate request ID
    std::string request_id = "req_c_" + std::to_string(request_counter_++);

    // Get data for this request (Green team data)
    auto all_data = getGreenTeamData(request->name());

    // Create chunks (e.g., 5 items per chunk for demonstration)
    const int CHUNK_SIZE = 5;
    std::deque<DataChunk> chunks;

    for (size_t i = 0; i < all_data.size(); i += CHUNK_SIZE) {
      DataChunk chunk;
      chunk.set_request_id(request_id);
      size_t end = std::min(i + CHUNK_SIZE, all_data.size());
      for (size_t j = i; j < end; ++j) {
        auto* data_item = chunk.add_data();
        data_item->CopyFrom(all_data[j]);
      }
      chunk.set_has_more_chunks((end < all_data.size()));
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

    std::cout << "Server C: Created " << chunks.size() << " chunks with "
              << all_data.size() << " total data items" << std::endl;

    return Status::OK;
  }

  Status GetNextChunk(ServerContext* context, const ChunkRequest* request,
                     DataChunk* reply) override {
    std::cout << "Server C: Get next chunk for: " << request->request_id() << std::endl;

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

    std::cout << "Server C: Returning chunk " << req_state.current_chunk_index
              << " of " << req_state.chunks.size() << std::endl;

    return Status::OK;
  }

  Status CancelRequest(ServerContext* context, const CancelRequestMessage* request,
                      Ack* reply) override {
    std::cout << "Server C: Cancel request: " << request->request_id() << std::endl;

    std::lock_guard<std::mutex> lock(requests_mutex_);
    chunked_requests_.erase(request->request_id());

    reply->set_success(true);
    return Status::OK;
  }

 private:
  void initializeRealData() {
    // Load real air quality data from Mini 2 data directory
    // Path is relative to the build directory
    std::string dataPath = "../data/air_quality";

    try {
      std::cout << "Server C: Loading real air quality data from: " << dataPath << std::endl;
      dataManager_.loadFromDirectoryParallel(dataPath, 4);  // Use parallel loading with 4 threads

      int totalReadings = dataManager_.getReadingCount();
      std::cout << "Server C: Loaded " << totalReadings << " air quality readings" << std::endl;

      // Convert to protobuf format and store
      auto allReadings = dataManager_.getAllReadings();
      for (const auto& reading : allReadings) {
        mini2::AirQualityData data;
        data.set_datetime(reading.getDatetime());
        data.set_timezone("UTC");  // Assuming UTC timezone
        data.set_location(reading.getSiteName());
        data.set_latitude(reading.getLatitude());
        data.set_longitude(reading.getLongitude());
        data.set_aqi_parameter(reading.getPollutantType());
        data.set_aqi_value(reading.getValue());
        data.set_aqi_unit(reading.getUnit());

        // Convert AQI category to string
        std::string category;
        switch (reading.getCategory()) {
          case 1: category = "Good"; break;
          case 2: category = "Moderate"; break;
          case 3: category = "Unhealthy for Sensitive Groups"; break;
          case 4: category = "Unhealthy"; break;
          case 5: category = "Very Unhealthy"; break;
          case 6: category = "Hazardous"; break;
          default: category = "Unknown"; break;
        }
        data.set_aqi_category(category);

        green_team_data_["real"].push_back(data);
      }

      std::cout << "Server C: Converted " << green_team_data_["real"].size()
                << " readings to protobuf format" << std::endl;

    } catch (const std::exception& e) {
      std::cerr << "Server C: Error loading real data: " << e.what() << std::endl;
      std::cerr << "Server C: Falling back to sample data" << std::endl;
      initializeSampleData();
    }
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

  std::unordered_map<std::string, std::vector<mini2::AirQualityData>> green_team_data_;
  int request_counter_ = 0;

  std::mutex requests_mutex_;
  std::unordered_map<std::string, ChunkedRequest> chunked_requests_;

  std::thread cleanup_thread_;
  std::atomic<bool> running_{true};

  AirQualityDataManager dataManager_;  // Mini 1 data manager
};

void RunServer() {
  std::string server_address("0.0.0.0:50053");
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