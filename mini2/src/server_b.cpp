#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>
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

const std::string SERVER_C_ADDRESS = "169.254.170.114:50053";
const std::string SERVER_B_PORT = "50052";

// Server B - Green Team Leader (Streaming/Pass-Through)
class GreenTeamLeaderImpl final : public DataService::Service
{
public:
  GreenTeamLeaderImpl() : mapping_manager_(std::make_unique<RequestMappingManager>())
  {
    std::cout << "Server B: Initializing Green Team Leader..." << std::endl;

    // Connect to Worker C
    std::string worker_c_address = SERVER_C_ADDRESS;
    worker_c_stub_ = DataService::NewStub(
        grpc::CreateChannel(worker_c_address, grpc::InsecureChannelCredentials()));

    std::cout << "Server B: Connected to worker C at " << worker_c_address << std::endl;
  }

  // Forwards the initial data request to Server C, stores a mapping between
  Status InitiateDataRequest(ServerContext *context, const Request *request,
                             DataChunk *reply) override
  {
    std::cout << "Server B (Green Leader): Received request for: "
              << request->name() << std::endl;

    // Generate our own request ID for tracking
    std::string our_request_id = CommonUtils::generateRequestId("req_b");
    std::cout << "Server B: Generated request ID: " << our_request_id << std::endl;

    // Forward request to Worker C
    ClientContext worker_context;
    DataChunk worker_response;

    std::cout << "Server B: Forwarding request to worker C..." << std::endl;
    Status status = worker_c_stub_->InitiateDataRequest(&worker_context, *request, &worker_response);

    if (!status.ok())
    {
      std::cerr << "Server B: ERROR - Worker C failed: "
                << status.error_message() << std::endl;

      reply->set_request_id(our_request_id);
      reply->set_has_more_chunks(false);

      return Status(grpc::INTERNAL, "Failed to retrieve data from worker C");
    }

    // Store mapping: our_request_id → C's request_id
    std::string worker_request_id = worker_response.request_id();
    mapping_manager_->storeMapping(our_request_id, worker_request_id);

    std::cout << "Server B: Mapped request [" << our_request_id
              << "] -> C's [" << worker_request_id << "]" << std::endl;
    std::cout << "Server B: Got first chunk with "
              << worker_response.data_size() << " items from C" << std::endl;

    // Return first chunk to Server A with OUR request ID
    reply->CopyFrom(worker_response);
    reply->set_request_id(our_request_id); // Replace C's ID with ours

    std::cout << "Server B: Returning first chunk with " << reply->data_size()
              << " items to Server A" << std::endl;
    std::cout << "Server B: Has more chunks: "
              << (reply->has_more_chunks() ? "Yes" : "No") << std::endl;

    return Status::OK;
  }

  // Forward GetNextChunk request to Server C using the stored request ID mapping
  Status GetNextChunk(ServerContext *context, const ChunkRequest *request,
                      DataChunk *reply) override
  {
    std::string our_request_id = request->request_id();
    std::cout << "Server B: Get next chunk for request ID: " << our_request_id << std::endl;

    // Look up C's request ID from our mapping
    std::string worker_request_id;
    if (!mapping_manager_->getWorkerRequestId(our_request_id, worker_request_id))
    {
      std::cerr << "Server B: ERROR - Request ID not found: "
                << our_request_id << std::endl;
      return Status(grpc::NOT_FOUND, "Request ID not found or expired");
    }

    std::cout << "Server B: Forwarding to C's request ID: " << worker_request_id << std::endl;

    // Forward GetNextChunk to Server C
    ChunkRequest worker_chunk_req;
    worker_chunk_req.set_request_id(worker_request_id);

    ClientContext worker_context;
    DataChunk worker_response;

    Status status = worker_c_stub_->GetNextChunk(&worker_context, worker_chunk_req, &worker_response);

    if (!status.ok())
    {
      std::cerr << "Server B: ERROR - Failed to get chunk from C: "
                << status.error_message() << std::endl;
      return status;
    }

    std::cout << "Server B: Got chunk with " << worker_response.data_size()
              << " items from C" << std::endl;

    // Return C's chunk to A (keeping our request ID)
    reply->CopyFrom(worker_response);
    reply->set_request_id(our_request_id); // Replace C's ID with ours

    std::cout << "Server B: Forwarding chunk with " << reply->data_size()
              << " items to Server A" << std::endl;
    std::cout << "Server B: Has more chunks: "
              << (reply->has_more_chunks() ? "Yes" : "No") << std::endl;

    // Clean up mapping if no more chunks
    if (!reply->has_more_chunks())
    {
      mapping_manager_->removeMapping(our_request_id);
      std::cout << "Server B: Request completed, removed mapping for: "
                << our_request_id << std::endl;
    }

    return Status::OK;
  }

  Status CancelRequest(ServerContext *context, const CancelRequestMessage *request,
                       Ack *reply) override
  {
    std::string our_request_id = request->request_id();
    std::cout << "Server B: Cancel request ID: " << our_request_id << std::endl;

    // Look up C's request ID
    std::string worker_request_id;
    if (mapping_manager_->getWorkerRequestId(our_request_id, worker_request_id))
    {
      mapping_manager_->removeMapping(our_request_id);
      std::cout << "Server B: Removed mapping for: " << our_request_id << std::endl;
    }
    else
    {
      std::cout << "Server B: Request ID not found (already expired?): "
                << our_request_id << std::endl;
      reply->set_success(true);
      return Status::OK;
    }

    // Forward cancel to Server C
    std::cout << "Server B: Forwarding cancel to C's request ID: "
              << worker_request_id << std::endl;

    CancelRequestMessage worker_cancel;
    worker_cancel.set_request_id(worker_request_id);

    ClientContext cancel_context;
    Ack cancel_ack;
    Status cancel_status = worker_c_stub_->CancelRequest(&cancel_context, worker_cancel, &cancel_ack);
    if (!cancel_status.ok())
    {
      std::cerr << "Server B: WARNING - Failed to cancel request on Server C: " << cancel_status.error_message() << std::endl;
      std::cerr << "Server B: C's request ID was: " << worker_request_id << std::endl;
    }

    reply->set_success(true);
    std::cout << "Server B: Cancel completed" << std::endl;

    return Status::OK;
  }

private:
  std::unique_ptr<DataService::Stub> worker_c_stub_;
  std::unique_ptr<RequestMappingManager> mapping_manager_;
};

void RunServerB()
{
  std::string server_address("0.0.0.0:" + SERVER_B_PORT);
  GreenTeamLeaderImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());

  std::cout << "========================================" << std::endl;
  std::cout << "Server B (Green Team Leader) listening on " << server_address << std::endl;
  std::cout << "Managing Green team: B (self), C (worker)" << std::endl;
  std::cout << "Mode: STREAMING (pass-through chunking)" << std::endl;
  std::cout << "========================================" << std::endl;

  server->Wait();
}

int main(int argc, char **argv)
{
  std::cout << "Server B (Green Team Leader) starting..." << std::endl;
  RunServerB();
  return 0;
}