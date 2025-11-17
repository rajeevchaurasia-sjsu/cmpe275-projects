#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include "dataserver.grpc.pb.h"
#include "dataserver.pb.h"
#include "CommonUtils.hpp"

using mini2::DataChunk;
using mini2::AirQualityData;

/**
 * Common session manager for worker servers (C, E, F)
 * Handles request sessions with automatic cleanup
 */
class SessionManager : public CleanupManager {
private:
  std::mutex mutex_;
  std::unordered_map<std::string, Session> sessions_;
  const size_t CHUNK_SIZE;

public:
  SessionManager(size_t chunk_size = 5)
    : CleanupManager(), CHUNK_SIZE(chunk_size) {}

  /**
   * Create a new session with data
   */
  std::string createSession(const std::vector<AirQualityData>& data) {
    std::string session_id = CommonUtils::generateRequestId("session");
    std::lock_guard<std::mutex> lock(mutex_);

    Session session;
    session.filtered_data = data;
    session.current_index = 0;
    sessions_[session_id] = session;

    return session_id;
  }

  /**
   * Get next chunk for a session
   */
  Status getNextChunk(const std::string& session_id, DataChunk* reply) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
      return Status(grpc::NOT_FOUND, "Session not found");
    }

    Session& session = it->second;
    session.last_access = std::chrono::steady_clock::now();

    // Calculate chunk range
    size_t start = session.current_index;
    size_t remaining = session.filtered_data.size() - start;
    size_t items_to_send = std::min(CHUNK_SIZE, remaining);

    // Create chunk
    *reply = CommonUtils::createChunk(session.filtered_data, session_id,
                                     start, items_to_send);

    // Update session state
    session.current_index += items_to_send;

    // Remove session if complete
    if (session.current_index >= session.filtered_data.size()) {
      sessions_.erase(it);
    }

    return Status::OK;
  }

  /**
   * Cancel a session
   */
  void cancelSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session_id);
  }

protected:
  void cleanupExpiredItems() override {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::minutes(30);

    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
      if (now - it->second.last_access > timeout) {
        it = sessions_.erase(it);
      } else {
        ++it;
      }
    }
  }
};

#endif