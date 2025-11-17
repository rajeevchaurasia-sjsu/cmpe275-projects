#ifndef COMMON_UTILS_HPP
#define COMMON_UTILS_HPP

#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include "dataserver.grpc.pb.h"
#include "dataserver.pb.h"

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

/**
 * Common structure to track chunked request state across all servers
 */
struct ChunkedRequest {
  std::deque<DataChunk> chunks;
  int current_chunk_index = 0;
  std::chrono::steady_clock::time_point last_access;

  ChunkedRequest() : last_access(std::chrono::steady_clock::now()) {}
};

/**
 * Common structure for worker servers to track request sessions
 */
struct Session {
  std::vector<AirQualityData> filtered_data;
  size_t current_index = 0;
  std::chrono::steady_clock::time_point last_access;

  Session() : last_access(std::chrono::steady_clock::now()) {}
};

class CommonUtils {
public:
  /**
   * Generate unique request ID with server prefix
   */
  static std::string generateRequestId(const std::string& prefix) {
    static std::atomic<int> counter(0);
    return prefix + "_" + std::to_string(++counter);
  }

  /**
   * Create a data chunk from a vector of AirQualityData
   */
  static DataChunk createChunk(const std::vector<AirQualityData>& data,
                               const std::string& request_id,
                               size_t start_idx, size_t chunk_size) {
    DataChunk chunk;
    chunk.set_request_id(request_id);

    size_t end_idx = std::min(start_idx + chunk_size, data.size());
    for (size_t i = start_idx; i < end_idx; ++i) {
      *chunk.add_data() = data[i];
    }

    chunk.set_has_more_chunks(end_idx < data.size());
    return chunk;
  }

  /**
   * Convert AirQualityReading to protobuf AirQualityData
   */
  template<typename ReadingType>
  static AirQualityData convertToProtobuf(const ReadingType& reading) {
    AirQualityData data;
    data.set_datetime(reading.getDatetime());
    data.set_timezone("UTC");
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

    return data;
  }
};

/**
 * Common cleanup manager for expired requests/sessions
 */
class CleanupManager {
private:
  std::thread cleanup_thread_;
  std::atomic<bool> running_;
  std::chrono::minutes expiry_time_;
  std::chrono::minutes cleanup_interval_;

public:
  CleanupManager(std::chrono::minutes expiry = std::chrono::minutes(30),
                 std::chrono::minutes interval = std::chrono::minutes(5))
    : running_(true), expiry_time_(expiry), cleanup_interval_(interval) {}

  virtual ~CleanupManager() {
    stop();
  }

  void start() {
    cleanup_thread_ = std::thread(&CleanupManager::cleanupLoop, this);
  }

  void stop() {
    running_ = false;
    if (cleanup_thread_.joinable()) {
      cleanup_thread_.join();
    }
  }

protected:
  virtual void cleanupExpiredItems() = 0;

private:
  void cleanupLoop() {
    while (running_) {
      std::this_thread::sleep_for(cleanup_interval_);
      cleanupExpiredItems();
    }
  }
};

/**
 * Common chunking manager for servers
 */
class ChunkingManager {
private:
  std::mutex mutex_;
  std::unordered_map<std::string, ChunkedRequest> chunked_requests_;
  std::unique_ptr<CleanupManager> cleanup_manager_;

public:
  ChunkingManager() {
    // Custom cleanup implementation for chunked requests
    class ChunkCleanup : public CleanupManager {
    private:
      ChunkingManager* parent_;
    public:
      ChunkCleanup(ChunkingManager* parent) : CleanupManager(), parent_(parent) {}
    protected:
      void cleanupExpiredItems() override {
        parent_->cleanupExpiredRequests();
      }
    };

    cleanup_manager_ = std::make_unique<ChunkCleanup>(this);
    cleanup_manager_->start();
  }

  ~ChunkingManager() {
    cleanup_manager_->stop();
  }

  /**
   * Store chunked request data
   */
  void storeChunks(const std::string& request_id, std::deque<DataChunk> chunks) {
    std::lock_guard<std::mutex> lock(mutex_);
    ChunkedRequest req_state;
    req_state.chunks = std::move(chunks);
    req_state.current_chunk_index = 0;
    chunked_requests_[request_id] = req_state;
  }

  /**
   * Get next chunk for a request
   */
  Status getNextChunk(const std::string& request_id, DataChunk* reply) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = chunked_requests_.find(request_id);
    if (it == chunked_requests_.end()) {
      return Status(grpc::NOT_FOUND, "Request ID not found");
    }

    ChunkedRequest& req_state = it->second;
    req_state.last_access = std::chrono::steady_clock::now();

    if (req_state.current_chunk_index + 1 >= static_cast<int>(req_state.chunks.size())) {
      return Status(grpc::OUT_OF_RANGE, "No more chunks available");
    }

    req_state.current_chunk_index++;
    *reply = req_state.chunks[req_state.current_chunk_index];

    return Status::OK;
  }

  /**
   * Cancel a request
   */
  void cancelRequest(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    chunked_requests_.erase(request_id);
  }

private:
  void cleanupExpiredRequests() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::minutes(30);

    for (auto it = chunked_requests_.begin(); it != chunked_requests_.end(); ) {
      if (now - it->second.last_access > timeout) {
        it = chunked_requests_.erase(it);
      } else {
        ++it;
      }
    }
  }
};

/**
 * Common structure for leader servers to track request mappings to workers
 */
struct RequestMapping {
  std::string worker_request_id;
  std::chrono::steady_clock::time_point last_access;

  RequestMapping() : last_access(std::chrono::steady_clock::now()) {}
  RequestMapping(const std::string& worker_id) : worker_request_id(worker_id), last_access(std::chrono::steady_clock::now()) {}
};

/**
 * Common request mapping manager for leader servers
 */
class RequestMappingManager {
private:
  std::mutex mutex_;
  std::unordered_map<std::string, RequestMapping> mappings_;
  std::unique_ptr<CleanupManager> cleanup_manager_;

public:
  RequestMappingManager() {
    // Custom cleanup implementation for mappings
    class MappingCleanup : public CleanupManager {
    private:
      RequestMappingManager* parent_;
    public:
      MappingCleanup(RequestMappingManager* parent) : CleanupManager(), parent_(parent) {}
    protected:
      void cleanupExpiredItems() override {
        parent_->cleanupExpiredMappings();
      }
    };

    cleanup_manager_ = std::make_unique<MappingCleanup>(this);
    cleanup_manager_->start();
  }

  ~RequestMappingManager() {
    cleanup_manager_->stop();
  }

  /**
   * Store a request mapping
   */
  void storeMapping(const std::string& leader_request_id, const std::string& worker_request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    mappings_[leader_request_id] = RequestMapping(worker_request_id);
  }

  /**
   * Get worker request ID for a leader request ID
   */
  bool getWorkerRequestId(const std::string& leader_request_id, std::string& worker_request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = mappings_.find(leader_request_id);
    if (it == mappings_.end()) {
      return false;
    }
    it->second.last_access = std::chrono::steady_clock::now();
    worker_request_id = it->second.worker_request_id;
    return true;
  }

  /**
   * Remove a mapping
   */
  void removeMapping(const std::string& leader_request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    mappings_.erase(leader_request_id);
  }

private:
  void cleanupExpiredMappings() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::minutes(30);

    for (auto it = mappings_.begin(); it != mappings_.end(); ) {
      if (now - it->second.last_access > timeout) {
        it = mappings_.erase(it);
      } else {
        ++it;
      }
    }
  }
};

#endif