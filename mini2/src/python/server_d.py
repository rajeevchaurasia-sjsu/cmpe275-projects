import grpc
from concurrent import futures
import sys
import os

SERVER_D_PORT = 50054
SERVER_E_ADDRESS = '169.254.156.148:50055'
SERVER_F_ADDRESS = '169.254.156.148:50056'

# Added the proto generated code to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'python_generated'))

import dataserver_pb2
import dataserver_pb2_grpc
from common_utils import CommonUtils, RequestMappingManager


class PinkTeamLeaderService(dataserver_pb2_grpc.DataServiceServicer):
    """
    Server D - Pink Team Leader
    Coordinates Workers E and F, merges their data
    """
    
    def __init__(self):
        print("Server D: Initializing Pink Team Leader...")
        self._mapping_manager = RequestMappingManager()
        self._mapping_manager.start()
        
        # Connecting to Worker E (C++ - Sept 1-15)
        print(f"Server D: Connecting to Worker E (C++) at {SERVER_E_ADDRESS}...")
        self.worker_e_address = SERVER_E_ADDRESS
        self.worker_e_channel = grpc.insecure_channel(self.worker_e_address)
        self.worker_e_stub = dataserver_pb2_grpc.DataServiceStub(self.worker_e_channel)
        
        # Connecting to Worker F (Python - Sept 16-30)
        print(f"Server D: Connecting to Worker F (Python) at {SERVER_F_ADDRESS}...")
        self.worker_f_address = SERVER_F_ADDRESS
        self.worker_f_channel = grpc.insecure_channel(self.worker_f_address)
        self.worker_f_stub = dataserver_pb2_grpc.DataServiceStub(self.worker_f_channel)
        
        print("Server D: Connected to both workers")
        print("Server D: Worker E handles Sept 1-15, Worker F handles Sept 16-30")
        print("Server D: Pink Team Leader initialized")
    
    def InitiateDataRequest(self, request, context):
        print(f"Server D (Pink Leader): Received request for: {request.name}")
        
        our_request_id = CommonUtils.generate_request_id("req_d")
        print(f"Server D: Generated request ID: {our_request_id}")
        
        # Forwarding request to BOTH workers in parallel
        print(f"Server D: Querying Worker E (Sept 1-15)...")
        try:
            e_response = self.worker_e_stub.InitiateDataRequest(request)
            print(f"Server D: Worker E responded, request_id: {e_response.request_id}, "
                  f"has_more: {e_response.has_more_chunks}, "
                  f"data_count: {len(e_response.data)}")
        except grpc.RpcError as e:
            print(f"Server D: ERROR - Worker E failed: {e.code()} - {e.details()}")
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details("Worker E unavailable")
            return dataserver_pb2.DataChunk()
        
        print(f"Server D: Querying Worker F (Sept 16-30)...")
        try:
            f_response = self.worker_f_stub.InitiateDataRequest(request)
            print(f"Server D: Worker F responded, request_id: {f_response.request_id}, "
                  f"has_more: {f_response.has_more_chunks}, "
                  f"data_count: {len(f_response.data)}")
        except grpc.RpcError as e:
            print(f"Server D: ERROR - Worker F failed: {e.code()} - {e.details()}")
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details("Worker F unavailable")
            return dataserver_pb2.DataChunk()
        
        self._mapping_manager.store_mapping(
            our_request_id,
            worker_e_request_id=e_response.request_id,
            worker_f_request_id=f_response.request_id
        )
        self._mapping_manager.update_e_has_more(our_request_id, e_response.has_more_chunks)
        self._mapping_manager.update_f_has_more(our_request_id, f_response.has_more_chunks)
        
        print(f"Server D: Merging chunks from E ({len(e_response.data)} items) "
              f"and F ({len(f_response.data)} items)")
        
        merged_response = dataserver_pb2.DataChunk()
        merged_response.request_id = our_request_id
        
        # Adding Worker E's data first (Sept 1-15)
        for item in e_response.data:
            merged_response.data.append(item)
        
        # Adding Worker F's data second (Sept 16-30)
        for item in f_response.data:
            merged_response.data.append(item)
        
        # We have more chunks if EITHER worker has more
        merged_response.has_more_chunks = (e_response.has_more_chunks or 
                                          f_response.has_more_chunks)
        
        print(f"Server D: Merged chunk contains {len(merged_response.data)} items, "
              f"has_more: {merged_response.has_more_chunks}")
        print(f"Server D: Returning merged chunk to Server A")
        
        return merged_response
    
    def GetNextChunk(self, request, context):
        """
        Get next chunk - queries BOTH workers and merges their chunks
        """
        our_request_id = request.request_id
        print(f"Server D: Get next chunk for request ID: {our_request_id}")
        
        # Look up both workers' request IDs
        e_request_id = self._mapping_manager.get_worker_e_request_id(our_request_id)
        f_request_id = self._mapping_manager.get_worker_f_request_id(our_request_id)
        
        if not e_request_id or not f_request_id:
            print(f"Server D: ERROR - Request ID not found: {our_request_id}")
            context.set_code(grpc.StatusCode.NOT_FOUND)
            context.set_details("Request ID not found or expired")
            return dataserver_pb2.DataChunk()
        
        # Check which workers have more chunks
        e_has_more = self._mapping_manager.get_e_has_more(our_request_id)
        f_has_more = self._mapping_manager.get_f_has_more(our_request_id)
        
        print(f"Server D: E has_more: {e_has_more}, F has_more: {f_has_more}")
        
        merged_response = dataserver_pb2.DataChunk()
        merged_response.request_id = our_request_id
        
        # Query Worker E if it has more chunks
        if e_has_more:
            print(f"Server D: Getting next chunk from Worker E...")
            try:
                e_chunk_request = dataserver_pb2.ChunkRequest()
                e_chunk_request.request_id = e_request_id
                
                e_response = self.worker_e_stub.GetNextChunk(e_chunk_request)
                print(f"Server D: Worker E returned {len(e_response.data)} items, "
                      f"has_more: {e_response.has_more_chunks}")
                
                # Add E's data to merged response
                for item in e_response.data:
                    merged_response.data.append(item)
                
                # Update E's status
                self._mapping_manager.update_e_has_more(our_request_id, e_response.has_more_chunks)
                
            except grpc.RpcError as e:
                print(f"Server D: ERROR - Worker E failed: {e.code()}")
        else:
            print(f"Server D: Worker E has no more chunks")
        
        # Query Worker F if it has more chunks
        if f_has_more:
            print(f"Server D: Getting next chunk from Worker F...")
            try:
                f_chunk_request = dataserver_pb2.ChunkRequest()
                f_chunk_request.request_id = f_request_id
                
                f_response = self.worker_f_stub.GetNextChunk(f_chunk_request)
                print(f"Server D: Worker F returned {len(f_response.data)} items, "
                      f"has_more: {f_response.has_more_chunks}")
                
                # Add F's data to merged response
                for item in f_response.data:
                    merged_response.data.append(item)
                
                # Update F's status
                self._mapping_manager.update_f_has_more(our_request_id, f_response.has_more_chunks)
                
            except grpc.RpcError as e:
                print(f"Server D: ERROR - Worker F failed: {e.code()}")
        else:
            print(f"Server D: Worker F has no more chunks")
        
        # Check if either worker still has more
        merged_response.has_more_chunks = self._mapping_manager.has_more_chunks(our_request_id)
        
        print(f"Server D: Merged chunk contains {len(merged_response.data)} items, "
              f"has_more: {merged_response.has_more_chunks}")
        
        return merged_response
    
    def CancelRequest(self, request, context):
        our_request_id = request.request_id
        print(f"Server D: Cancel request ID: {our_request_id}")
        
        e_request_id = self._mapping_manager.get_worker_e_request_id(our_request_id)
        f_request_id = self._mapping_manager.get_worker_f_request_id(our_request_id)
        
        if e_request_id or f_request_id:
            self._mapping_manager.remove_mapping(our_request_id)
            print(f"Server D: Removed mapping for: {our_request_id}")
        else:
            print(f"Server D: Request ID not found (already expired?): {our_request_id}")
        
        if e_request_id:
            print(f"Server D: Canceling Worker E request: {e_request_id}")
            try:
                cancel_msg = dataserver_pb2.CancelRequestMessage()
                cancel_msg.request_id = e_request_id
                e_ack = self.worker_e_stub.CancelRequest(cancel_msg)
                print(f"Server D: Worker E cancel {'succeeded' if e_ack.success else 'failed'}")
            except grpc.RpcError as e:
                print(f"Server D: Worker E cancel failed: {e.code()}")
        
        if f_request_id:
            print(f"Server D: Canceling Worker F request: {f_request_id}")
            try:
                cancel_msg = dataserver_pb2.CancelRequestMessage()
                cancel_msg.request_id = f_request_id
                f_ack = self.worker_f_stub.CancelRequest(cancel_msg)
                print(f"Server D: Worker F cancel {'succeeded' if f_ack.success else 'failed'}")
            except grpc.RpcError as e:
                print(f"Server D: Worker F cancel failed: {e.code()}")
        
        response = dataserver_pb2.Ack()
        response.success = True
        
        return response
    
    def shutdown(self):
        print("Server D: Shutting down...")
        self._mapping_manager.stop()
        
        self.worker_e_channel.close()
        self.worker_f_channel.close()


def serve():
    """Start the gRPC server"""
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    
    service = PinkTeamLeaderService()
    dataserver_pb2_grpc.add_DataServiceServicer_to_server(service, server)
    
    server_address = f'0.0.0.0:{SERVER_D_PORT}'
    server.add_insecure_port(server_address)
    
    server.start()
    
    print("=" * 60)
    print(f"Server D (Pink Team Leader) listening on {server_address}")
    print("Managing Pink team: D (self), E (worker), F (worker)")
    print("Mode: STREAMING with DATA MERGING")
    print("Worker E: September 1-15 (first half)")
    print("Worker F: September 16-30 (second half)")
    print("Server D: Merges both workers' data streams")
    print("=" * 60)
    
    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        print("\nServer D: Received shutdown signal")
        service.shutdown()
        server.stop(grace=5)


if __name__ == '__main__':
    print("Server D (Pink Team Leader) starting...")
    serve()