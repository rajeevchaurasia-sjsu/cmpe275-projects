import grpc
from concurrent import futures
import time
import os
import sys
import csv
import glob

# Add the protos directory to the path so we can import the generated modules
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'python_generated'))

# Import common utilities
from common_utils import CommonUtils, SessionManager

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
        self.session_manager = SessionManager()
        self.load_dummy_data()
        print(f"[Server F] Initialized with {len(self.data_store)} records.")

    def load_dummy_data(self):
        """
        Load real air quality data from CSV files.
        Server F loads data from Sept 16-30 (folders 20200916 to 20200930)
        """
        # Get path to data directory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        data_dir = os.path.join(script_dir, '../../data/air_quality')
        
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
                                continue
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
        
        # Generate Request ID using common utilities
        request_id = CommonUtils.generate_request_id("F")
        
        # Create session with all data
        self.session_manager.create_session(request_id, self.data_store)
        
        # Get first chunk
        chunk_data, has_more_chunks = self.session_manager.get_next_chunk(request_id)
        
        # Create response
        response = dataserver_pb2.DataChunk()
        response.request_id = request_id
        response.data.extend(chunk_data)
        response.has_more_chunks = has_more_chunks
        
        return response

    def GetNextChunk(self, request, context):
        req_id = request.request_id
        
        try:
            chunk_data, has_more_chunks = self.session_manager.get_next_chunk(req_id)
            
            # Create response
            response = dataserver_pb2.DataChunk()
            response.request_id = req_id
            response.data.extend(chunk_data)
            response.has_more_chunks = has_more_chunks
            
            print(f"[Server F] Sending Chunk for {req_id}")
            return response
        except ValueError as e:
            context.set_code(grpc.StatusCode.NOT_FOUND)
            context.set_details(str(e))
            return dataserver_pb2.DataChunk()

    def CancelRequest(self, request, context):
        req_id = request.request_id
        print(f"[Server F] Cancelling request {req_id}")
        
        success = self.session_manager.cancel_request(req_id)
        return dataserver_pb2.Ack(success=success)

    def shutdown(self):
        """Shutdown the session manager"""
        self.session_manager.stop()

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    servicer = DataServiceServicer()
    dataserver_pb2_grpc.add_DataServiceServicer_to_server(servicer, server)
    
    # Listen on port 50056 (Computer 3, Worker F)
    server.add_insecure_port(f'[::]:{SERVER_PORT}')
    print(f"Server F (Python) started on port {SERVER_PORT}")
    
    server.start()
    try:
        while True:
            time.sleep(86400)
    except KeyboardInterrupt:
        print("[Server F] Shutting down...")
        servicer.shutdown()
        server.stop(0)

if __name__ == '__main__':
    serve()