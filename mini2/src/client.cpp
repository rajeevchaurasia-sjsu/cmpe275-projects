#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "dataserver.grpc.pb.h"
#include "dataserver.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using mini2::DataService;
using mini2::Request;
using mini2::DataChunk;
using mini2::ChunkRequest;
using mini2::CancelRequestMessage;
using mini2::Ack;

class DataServiceClient {
 public:
  DataServiceClient(std::shared_ptr<Channel> channel)
      : stub_(DataService::NewStub(channel)) {}

  void InitiateRequest(const std::string& query) {
    Request request;
    request.set_name(query);

    DataChunk reply;
    ClientContext context;

    Status status = stub_->InitiateDataRequest(&context, request, &reply);

    if (status.ok()) {
      std::cout << "Client: Received first chunk, request_id: " << reply.request_id()
                << ", has_more: " << reply.has_more_chunks() << std::endl;

      // TODO: Handle chunked response
    } else {
      std::cout << "Client: Request failed: " << status.error_message() << std::endl;
    }
  }

 private:
  std::unique_ptr<DataService::Stub> stub_;
};

int main(int argc, char** argv) {
  std::string target_str = "localhost:50051";
  if (argc > 1) {
    target_str = argv[1];
  }

  DataServiceClient client(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

  std::string query = "test_query";
  if (argc > 2) {
    query = argv[2];
  }

  client.InitiateRequest(query);

  return 0;
}