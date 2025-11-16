import grpc
from concurrent import futures
import sys
import os
import threading
import time
from datetime import datetime, timedelta

# Add python_generated to path so we can import our proto files
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'python_generated'))

import dataserver_pb2
import dataserver_pb2_grpc


class RequestMapping:
    """
    Stores mapping between our request ID and worker's request ID
    Similar to C++ RequestMapping struct
    """
    def __init__(self, worker_request_id):
        self.worker_request_id = worker_request_id
        self.last_access = datetime.now()


class PinkTeamLeaderService(dataserver_pb2_grpc.DataServiceServicer):
    """
    Server D - Pink Team Leader
    Similar to Server B (Green Team Leader) but for Pink team
    Will coordinate with workers E and F
    """
    
    def __init__(self):
        print("Server D: Initializing Pink Team Leader...")
        
        # Thread-safe request ID counter (like C++ atomic<int>)
        self._request_counter = 0
        self._counter_lock = threading.Lock()
        
        # Request mappings: our_request_id -> RequestMapping
        self._request_mappings = {}
        self._mappings_lock = threading.Lock()
        
        # Cleanup configuration
        self.REQUEST_EXPIRY_TIME = timedelta(minutes=30)
        self.CLEANUP_INTERVAL = timedelta(minutes=5)
        
        # Start cleanup thread
        self._running = True
        self._cleanup_thread = threading.Thread(target=self._cleanup_expired_mappings, daemon=True)
        self._cleanup_thread.start()
        
        # TODO: Connect to workers E and F
        # For now, we'll set these to None
        self.worker_e_stub = None
        self.worker_f_stub = None
        
        print("Server D: Pink Team Leader initialized")
    
    def _generate_request_id(self):
        """
        Generate unique request ID in thread-safe manner
        Similar to C++ generateRequestId() with atomic counter
        """
        with self._counter_lock:
            self._request_counter += 1
            return f"req_d_pink_{self._request_counter}"
    
    def _cleanup_expired_mappings(self):
        """
        Background thread to remove expired request mappings
        Similar to C++ cleanupExpiredMappings()
        """
        while self._running:
            time.sleep(self.CLEANUP_INTERVAL.total_seconds())
            
            now = datetime.now()
            with self._mappings_lock:
                expired_keys = [
                    req_id for req_id, mapping in self._request_mappings.items()
                    if now - mapping.last_access > self.REQUEST_EXPIRY_TIME
                ]
                
                for req_id in expired_keys:
                    print(f"Server D: Cleaning up expired mapping: {req_id}")
                    del self._request_mappings[req_id]
    
    def InitiateDataRequest(self, request, context):
        """
        Handle initial data request from Server A
        Returns first chunk and stores mapping for GetNextChunk calls
        """
        print(f"Server D (Pink Leader): Received request for: {request.name}")
        
        # Generate our own request ID for tracking
        our_request_id = self._generate_request_id()
        print(f"Server D: Generated request ID: {our_request_id}")
        
        # TODO: Forward request to worker E or F
        # For now, create empty response
        response = dataserver_pb2.DataChunk()
        response.request_id = our_request_id
        response.has_more_chunks = False
        
        print(f"Server D: Returning first chunk (placeholder) to Server A")
        
        return response
    
    def GetNextChunk(self, request, context):
        """
        Handle request for next chunk of data
        Looks up worker's request ID and forwards the call
        """
        our_request_id = request.request_id
        print(f"Server D: Get next chunk for request ID: {our_request_id}")
        
        # Look up worker's request ID from our mapping
        with self._mappings_lock:
            if our_request_id not in self._request_mappings:
                print(f"Server D: ERROR - Request ID not found: {our_request_id}")
                context.set_code(grpc.StatusCode.NOT_FOUND)
                context.set_details("Request ID not found or expired")
                return dataserver_pb2.DataChunk()
            
            mapping = self._request_mappings[our_request_id]
            worker_request_id = mapping.worker_request_id
            
            # Update last access time
            mapping.last_access = datetime.now()
        
        print(f"Server D: Forwarding to worker's request ID: {worker_request_id}")
        
        # TODO: Forward GetNextChunk to worker
        # For now, return empty response
        response = dataserver_pb2.DataChunk()
        response.request_id = our_request_id
        response.has_more_chunks = False
        
        return response
    
    def CancelRequest(self, request, context):
        """
        Handle request cancellation
        Forwards cancel to worker and removes mapping
        """
        our_request_id = request.request_id
        print(f"Server D: Cancel request ID: {our_request_id}")
        
        # Look up worker's request ID
        worker_request_id = None
        with self._mappings_lock:
            if our_request_id in self._request_mappings:
                worker_request_id = self._request_mappings[our_request_id].worker_request_id
                del self._request_mappings[our_request_id]
                print(f"Server D: Removed mapping for: {our_request_id}")
            else:
                print(f"Server D: Request ID not found (already expired?): {our_request_id}")
        
        if worker_request_id:
            print(f"Server D: Forwarding cancel to worker's request ID: {worker_request_id}")
            # TODO: Forward cancel to worker
        
        response = dataserver_pb2.Ack()
        response.success = True
        
        return response
    
    def shutdown(self):
        """
        Cleanup when server stops
        """
        print("Server D: Shutting down...")
        self._running = False
        if self._cleanup_thread.is_alive():
            self._cleanup_thread.join(timeout=2)


def serve():
    """
    Start the gRPC server
    """
    # Create server with thread pool
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    
    # Register our service
    service = PinkTeamLeaderService()
    dataserver_pb2_grpc.add_DataServiceServicer_to_server(service, server)
    
    # Listen on port 50054
    server_address = '0.0.0.0:50054'
    server.add_insecure_port(server_address)
    
    # Start server
    server.start()
    
    print("=" * 40)
    print(f"Server D (Pink Team Leader) listening on {server_address}")
    print("Managing Pink team: D (self), E (worker), F (worker)")
    print("Mode: STREAMING (pass-through chunking)")
    print("=" * 40)
    
    try:
        # Keep server running
        server.wait_for_termination()
    except KeyboardInterrupt:
        print("\nServer D: Received shutdown signal")
        service.shutdown()
        server.stop(grace=5)


if __name__ == '__main__':
    print("Server D (Pink Team Leader) starting...")
    serve()