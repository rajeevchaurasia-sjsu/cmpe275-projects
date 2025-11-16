import grpc
from concurrent import futures
import time
import uuid
import os
import sys
import csv
import glob

# Add the protos directory to the path so we can import the generated modules
sys.path.append(os.path.join(os.path.dirname(__file__), '../protos'))

# Note: User must generate these using grpc_tools.protoc
# python -m grpc_tools.protoc -I../protos --python_out=. --grpc_python_out=. ../protos/dataserver.proto
try:
    import dataserver_pb2
    import dataserver_pb2_grpc
except ImportError:
    print("Error: Protobuf modules not found. Please compile dataserver.proto for Python.")
    sys.exit(1)

# Configuration
SERVER_PORT = 50056
CHUNK_SIZE = 5

class DataServiceServicer(dataserver_pb2_grpc.DataServiceServicer):
    def __init__(self):
        self.data_store = []
        self.sessions = {}
        self.load_dummy_data()
        print(f"[Server F] Initialized with {len(self.data_store)} records.")

    def load_dummy_data(self):
        """
        Load real air quality data from CSV files.
        Server F loads data from Sept 16-30 (folders 20200916 to 20200930)
        """
        # Get path to data directory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        data_dir = os.path.join(script_dir, '../data/air_quality')
        
        if not os.path.exists(data_dir):
            print(f"[Server F] Warning: Data directory not found at {data_dir}")
            print("[Server F] Using minimal dummy data instead")
            self._load_minimal_dummy_data()
            return
        
        print(f"[Server F] Loading data from Sept 16-30 (20200916 to 20200930)...")
        
        # Load specific date folders for Server F (Sept 16-30)
        date_folders = []
        for day in range(16, 31):  # Days 16 to 30
            folder_name = f"202009{day:02d}"
            folder_path = os.path.join(data_dir, folder_name)
            if os.path.exists(folder_path):
                date_folders.append(folder_path)
            else:
                print(f"[Server F] Warning: Folder not found: {folder_path}")
        
        if not date_folders:
            print(f"[Server F] Warning: No date folders found for Sept 16-30")
            print("[Server F] Using minimal dummy data instead")
            self._load_minimal_dummy_data()
            return
        
        # Load all CSV files from the specified date folders
        records_loaded = 0
        for folder_path in date_folders:
            csv_files = glob.glob(os.path.join(folder_path, '*.csv'))
            
            for csv_file in csv_files:
                try:
                    with open(csv_file, 'r') as f:
                        reader = csv.reader(f)
                        for row in reader:
                            # CSV format (no header): lat,lon,datetime,pollutant,value,unit,rawConc,aqi,category,siteName,agency,siteId,fullSiteId
                            if len(row) != 13:
                                continue  # Skip malformed rows
                            
                            try:
                                # Parse CSV row by position
                                record = dataserver_pb2.AirQualityData(
                                    datetime=row[2],  # ISO datetime
                                    timezone="UTC",
                                    location=row[9],  # siteName
                                    latitude=float(row[0]),
                                    longitude=float(row[1]),
                                    aqi_parameter=row[3],  # pollutant
                                    aqi_value=float(row[4]),  # value
                                    aqi_unit=row[5],  # unit
                                    aqi_category=self._get_category(int(row[7]))  # aqi
                                )
                                self.data_store.append(record)
                                records_loaded += 1
                            except (ValueError, IndexError) as e:
                                continue  # Skip malformed rows
                except Exception as e:
                    print(f"[Server F] Error reading {csv_file}: {e}")
                    continue
        
        print(f"[Server F] Loaded {len(self.data_store)} records from Sept 16-30 ({len(date_folders)} date folders)")
    
    def _load_minimal_dummy_data(self):
        """Fallback: Load minimal dummy data if CSV files not found"""
        for i in range(10):
            record = dataserver_pb2.AirQualityData(
                datetime="2020-09-01 12:00:00",
                timezone="-07:00",
                location=f"Sensor-F-{i}",
                latitude=37.3382 + (i * 0.01),
                longitude=-121.8863 + (i * 0.01),
                aqi_parameter="PM2.5",
                aqi_value=15.5 + i,
                aqi_unit="µg/m³",
                aqi_category="Good"
            )
            self.data_store.append(record)
    
    def _get_category(self, aqi):
        """Convert AQI value to category string"""
        if aqi <= 50:
            return "Good"
        elif aqi <= 100:
            return "Moderate"
        elif aqi <= 150:
            return "Unhealthy for Sensitive Groups"
        elif aqi <= 200:
            return "Unhealthy"
        elif aqi <= 300:
            return "Very Unhealthy"
        else:
            return "Hazardous"

    def InitiateDataRequest(self, request, context):
        print(f"[Server F] Received InitiateDataRequest for: {request.name}")
        
        # Generate Request ID
        request_id = str(uuid.uuid4())
        
        # In a real app, filter self.data_store based on request.name
        # Here, we just create a session pointing to all data
        self.sessions[request_id] = {
            "index": 0,
            "data": self.data_store  # Storing reference to data
        }
        
        # Prepare first chunk
        response = dataserver_pb2.DataChunk()
        response.request_id = request_id
        
        # Slice data
        chunk_data = self.data_store[0:CHUNK_SIZE]
        response.data.extend(chunk_data)
        
        # Update Index
        self.sessions[request_id]["index"] = len(chunk_data)
        
        # Has more?
        response.has_more_chunks = self.sessions[request_id]["index"] < len(self.data_store)
        
        return response

    def GetNextChunk(self, request, context):
        req_id = request.request_id
        
        if req_id not in self.sessions:
            context.set_code(grpc.StatusCode.NOT_FOUND)
            context.set_details("Session not found")
            return dataserver_pb2.DataChunk()
            
        session = self.sessions[req_id]
        start_idx = session["index"]
        end_idx = start_idx + CHUNK_SIZE
        
        # Slice next chunk
        chunk_data = session["data"][start_idx:end_idx]
        
        # Prepare Response
        response = dataserver_pb2.DataChunk()
        response.request_id = req_id
        response.data.extend(chunk_data)
        
        # Update state
        session["index"] = start_idx + len(chunk_data)
        response.has_more_chunks = session["index"] < len(session["data"])
        
        print(f"[Server F] Sending Chunk for {req_id} ({session['index']}/{len(session['data'])})")
        
        # Cleanup
        if not response.has_more_chunks:
            print(f"[Server F] Finished {req_id}. Cleaning up.")
            del self.sessions[req_id]
            
        return response

    def CancelRequest(self, request, context):
        req_id = request.request_id
        print(f"[Server F] Cancelling request {req_id}")
        
        success = False
        if req_id in self.sessions:
            del self.sessions[req_id]
            success = True
            
        return dataserver_pb2.Ack(success=success)

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    dataserver_pb2_grpc.add_DataServiceServicer_to_server(DataServiceServicer(), server)
    
    # Listen on port 50056 (Computer 3, Worker F)
    server.add_insecure_port(f'[::]:{SERVER_PORT}')
    print(f"Server F (Python) started on port {SERVER_PORT}")
    
    server.start()
    try:
        while True:
            time.sleep(86400)
    except KeyboardInterrupt:
        server.stop(0)

if __name__ == '__main__':
    serve()