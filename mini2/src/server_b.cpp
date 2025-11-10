#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <unordered_map>
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

// Server B - Green Team Leader
// TODO: Implement coordination with C, handle Green team requests

class GreenTeamLeaderImpl final : public DataService::Service {
 public:
  GreenTeamLeaderImpl() {
    std::cout << "Server B: Initializing Green Team Leader..." << std::endl;

    // Connect to Worker C
    std::string worker_c_address = "localhost:50053";
    worker_c_stub_ = DataService::NewStub(
        grpc::CreateChannel(worker_c_address, grpc::InsecureChannelCredentials())
    );

    std::cout << "Server B: Connecting to worker C at " << worker_c_address << std::endl;
  }

 private:
  std::unique_ptr<DataService::Stub> worker_c_stub_;
  std::mutex requests_mutex_;
  std::unordered_map<std::string, DataChunk> active_requests_;
};

void RunServerB() {
  std::string server_address("0.0.0.0:50052");
  
  // Create the service object
  GreenTeamLeaderImpl service;
  
  // Test 1: Can we create a Request object?
  Request test_request;
  test_request.set_name("test");
  std::cout << "✓ Request object works. Name: " << test_request.name() << std::endl;
  
  // Test 2: Can we create a DataChunk object?
  DataChunk test_chunk;
  test_chunk.set_request_id("test_123");
  std::cout << "✓ DataChunk object works. ID: " << test_chunk.request_id() << std::endl;
  
  // Test 3: Can we use mutex?
  std::mutex test_mutex;
  std::lock_guard<std::mutex> lock(test_mutex);
  std::cout << "✓ Mutex works" << std::endl;
  
  // Test 4: Can we use unordered_map?
  std::unordered_map<std::string, int> test_map;
  test_map["key1"] = 100;
  std::cout << "✓ Unordered_map works. Value: " << test_map["key1"] << std::endl;
  
  std::cout << "\n✅ All Step 1 libraries working correctly!" << std::endl;
}

int main(int argc, char** argv) {
  std::cout << "Server B (Green Team Leader) starting..." << std::endl;
  RunServerB();
  return 0;
}