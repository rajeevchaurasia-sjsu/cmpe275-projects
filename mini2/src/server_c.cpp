#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <vector>
#include <unordered_map>
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

// Server C - Green Team Worker
// Processes data requests for the Green team
class WorkerServiceImpl final : public DataService::Service {
 public:
  WorkerServiceImpl() {
    // Initialize some sample data for Green team
    initializeSampleData();
  }

  Status InitiateDataRequest(ServerContext* context, const Request* request,
                           DataChunk* reply) override {
    std::cout << "Server C (Green Worker): Processing request for: " << request->name() << std::endl;

    // Generate request ID
    std::string request_id = "req_c_" + std::to_string(request_counter_++);

    // Get data for this request (Green team data)
    auto data = getGreenTeamData(request->name());

    // Create chunk with data
    for (const auto& item : data) {
      auto* data_item = reply->add_data();
      data_item->CopyFrom(item);
    }

    reply->set_request_id(request_id);
    reply->set_has_more_chunks(false);  // Single chunk for now

    std::cout << "Server C: Returning " << data.size() << " data items" << std::endl;

    return Status::OK;
  }

  Status GetNextChunk(ServerContext* context, const ChunkRequest* request,
                     DataChunk* reply) override {
    std::cout << "Server C: Get next chunk for: " << request->request_id() << std::endl;

    // For now, no more chunks available
    reply->set_request_id(request->request_id());
    reply->set_has_more_chunks(false);

    return Status::OK;
  }

  Status CancelRequest(ServerContext* context, const CancelRequestMessage* request,
                      Ack* reply) override {
    std::cout << "Server C: Cancel request: " << request->request_id() << std::endl;

    reply->set_success(true);
    return Status::OK;
  }

 private:
  void initializeSampleData() {
    // Create sample Green team data
    AirQualityData data1;
    data1.set_datetime("2020-08-10T12:00:00Z");
    data1.set_timezone("UTC");
    data1.set_location("Green City 1");
    data1.set_latitude(37.7749);
    data1.set_longitude(-122.4194);
    data1.set_aqi_parameter("PM2.5");
    data1.set_aqi_value(25.5);
    data1.set_aqi_unit("µg/m³");
    data1.set_aqi_category("Good");

    AirQualityData data2;
    data2.set_datetime("2020-08-10T13:00:00Z");
    data2.set_timezone("UTC");
    data2.set_location("Green City 2");
    data2.set_latitude(37.7849);
    data2.set_longitude(-122.4294);
    data2.set_aqi_parameter("PM10");
    data2.set_aqi_value(45.2);
    data2.set_aqi_unit("µg/m³");
    data2.set_aqi_category("Moderate");

    green_team_data_["sample"] = {data1, data2};
  }

  std::vector<AirQualityData> getGreenTeamData(const std::string& query) {
    // For now, return sample data regardless of query
    auto it = green_team_data_.find("sample");
    if (it != green_team_data_.end()) {
      return it->second;
    }
    return {};
  }

  std::unordered_map<std::string, std::vector<AirQualityData>> green_team_data_;
  int request_counter_ = 0;
};

void RunServer() {
  std::string server_address("0.0.0.0:50053");
  WorkerServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server C (Green Worker) listening on " << server_address << std::endl;
  std::cout << "Serving Green team data" << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();
  return 0;
}