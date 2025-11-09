import grpc
from concurrent import futures
import dataserver_pb2
import dataserver_pb2_grpc

class DataServiceServicer(dataserver_pb2_grpc.DataServiceServicer):
    def InitiateDataRequest(self, request, context):
        print(f"Server F (Python): Received request for: {request.name}")
        # TODO: Implement request handling
        chunk = dataserver_pb2.DataChunk()
        chunk.request_id = "req_python_123"
        chunk.has_more_chunks = True
        return chunk

    def GetNextChunk(self, request, context):
        print(f"Server F (Python): Get next chunk for: {request.request_id}")
        # TODO: Implement chunk retrieval
        chunk = dataserver_pb2.DataChunk()
        chunk.request_id = request.request_id
        chunk.has_more_chunks = False
        return chunk

    def CancelRequest(self, request, context):
        print(f"Server F (Python): Cancel request: {request.request_id}")
        # TODO: Implement cancellation
        ack = dataserver_pb2.Ack()
        ack.success = True
        return ack

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    dataserver_pb2_grpc.add_DataServiceServicer_to_server(DataServiceServicer(), server)
    server.add_insecure_port('[::]:50056')  # Different port for Python server
    server.start()
    print("Server F (Python Pink Worker) listening on [::]:50056")
    server.wait_for_termination()

if __name__ == '__main__':
    serve()