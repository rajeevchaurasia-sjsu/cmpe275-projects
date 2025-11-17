#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "dataserver.grpc.pb.h"
#include "dataserver.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using mini2::Ack;
using mini2::CancelRequestMessage;
using mini2::ChunkRequest;
using mini2::DataChunk;
using mini2::DataService;
using mini2::Request;

const std::string SERVER_A_ADDRESS = "192.168.156.148:50051";

class DataServiceClient
{
public:
  DataServiceClient(std::shared_ptr<Channel> channel)
      : stub_(DataService::NewStub(channel)) {}

  void InitiateRequest(const std::string &query)
  {
    Request request;
    request.set_name(query);

    DataChunk reply;
    ClientContext context;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Client: Sending request: \"" << query << "\"" << std::endl;
    std::cout << "========================================\n"
              << std::endl;

    Status status = stub_->InitiateDataRequest(&context, request, &reply);

    if (status.ok())
    {
      std::cout << "✅ Client: SUCCESS - Received first chunk" << std::endl;
      std::cout << "   Request ID: " << reply.request_id() << std::endl;
      std::cout << "   Items in chunk: " << reply.data_size() << std::endl;
      std::cout << "   Has more chunks: " << (reply.has_more_chunks() ? "Yes" : "No") << std::endl;

      // Display first chunk data
      displayChunkData(reply, 1);

      // Collect all chunks if available
      int total_items = reply.data_size();
      int chunk_count = 1;
      std::string request_id = reply.request_id();

      while (reply.has_more_chunks())
      {
        chunk_count++;
        if (getNextChunk(request_id, reply))
        {
          total_items += reply.data_size();
          displayChunkData(reply, chunk_count);
        }
        else
        {
          std::cerr << "❌ Failed to get chunk " << chunk_count << std::endl;
          break;
        }
      }

      std::cout << "\n========================================" << std::endl;
      std::cout << "✅ Client: Request Complete!" << std::endl;
      std::cout << "   Total chunks received: " << chunk_count << std::endl;
      std::cout << "   Total items received: " << total_items << std::endl;
      std::cout << "========================================\n"
                << std::endl;
    }
    else
    {
      std::cerr << "❌ Client: Request failed!" << std::endl;
      std::cerr << "   Error code: " << status.error_code() << std::endl;
      std::cerr << "   Error message: " << status.error_message() << std::endl;
    }
  }

  bool getNextChunk(const std::string &request_id, DataChunk &reply)
  {
    ChunkRequest chunk_req;
    chunk_req.set_request_id(request_id);

    ClientContext context;
    Status status = stub_->GetNextChunk(&context, chunk_req, &reply);

    return status.ok();
  }

  void displayChunkData(const DataChunk &chunk, int chunk_number)
  {
    std::cout << "\n--- Chunk " << chunk_number << " ---" << std::endl;
    std::cout << "Items: " << chunk.data_size() << std::endl;

    // Show first 3 items from this chunk
    int display_count = std::min(3, chunk.data_size());
    for (int i = 0; i < display_count; i++)
    {
      const auto &data = chunk.data(i);
      std::cout << "  [" << i << "] ";

      // Use the CORRECT field names from protobuf
      if (!data.location().empty())
      {
        std::cout << data.location();
      }

      if (data.aqi_value() > 0)
      {
        std::cout << ", AQI: " << data.aqi_value();
      }

      if (!data.aqi_category().empty())
      {
        std::cout << ", Category: " << data.aqi_category();
      }

      if (!data.datetime().empty())
      {
        std::cout << ", Time: " << data.datetime();
      }

      std::cout << std::endl;
    }

    if (chunk.data_size() > 3)
    {
      std::cout << "  ... and " << (chunk.data_size() - 3) << " more items" << std::endl;
    }
  }

private:
  std::unique_ptr<DataService::Stub> stub_;
};

int main(int argc, char **argv)
{
  std::string target_str = SERVER_A_ADDRESS;
  if (argc > 1)
  {
    target_str = argv[1];
  }

  std::cout << "========================================" << std::endl;
  std::cout << "Air Quality Data Service - Test Client" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "Connecting to: " << target_str << std::endl;

  DataServiceClient client(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

  std::string query = "green_data";
  if (argc > 2)
  {
    query = argv[2];
  }

  client.InitiateRequest(query);

  return 0;
}