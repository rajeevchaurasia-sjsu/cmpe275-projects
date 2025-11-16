# Mini 2 - Multi-Process Data Service

## Implementation Status
- ✅ **Server A (Leader)**: Implemented - routes requests to team leaders B and D
- ✅ **Server C (Green Worker)**: Implemented - serves sample Green team data
- ✅ **Server B (Green Team Leader)**: Implemented - streaming pass-through architecture
- ✅ **Server D (Pink Team Leader)**: Implemented (Python) - streaming pass-through architecture
- 🔄 **Server E (Pink Worker)**: In progress
- 🔄 **Server F (Pink Worker)**: In progress
- ✅ **Client**: Basic implementation available

## Team Setup
- **Person 1 (Computer 1)**: Server A (Leader), Server C (Green Worker) ✅
- **Person 2 (Computer 2)**: Server B (Green Team Leader) ✅, Server D (Pink Team Leader) ✅
- **Person 3 (Computer 3)**: Server E (Pink Worker), Server F (Pink Worker)

## Network Configuration
1. Connect laptops via Ethernet cable
2. Configure static IPs in the 192.168.1.x range
3. Update `network_config.txt` with actual IPs
4. Each computer runs 2 server processes

## Technology Stack
- **C++ Servers**: A, B, C, E (using gRPC C++)
- **Python Server**: D, F (using gRPC Python)
- **Client**: C++ (gRPC C++)

## Prerequisites

### For C++ Servers (A, B, C)
```bash
# Install dependencies (macOS)
brew install grpc protobuf cmake

# Or (Linux)
sudo apt-get install -y build-essential cmake libgrpc++-dev protobuf-compiler-grpc
```

### For Python Server (D)
```bash
# Create virtual environment
cd mini2
python3 -m venv .venv
source .venv/bin/activate  # On macOS/Linux
# Or: .venv\Scripts\activate  # On Windows

# Install dependencies
pip install -r requirements.txt

# Generate Python protobuf code
python -m grpc_tools.protoc \
  -I./protos \
  --python_out=./python_generated \
  --grpc_python_out=./python_generated \
  ./protos/dataserver.proto
```

## Build Instructions

### C++ Servers
```bash
cd build
cmake ..
make
```

### Python Server (No Build Needed)
```bash
# Just ensure venv is activated and dependencies installed
source .venv/bin/activate
```

## Running Servers

### Computer 1 (Person 1)
```bash
cd build
./server_a &   # Port 50051 - Leader
./server_c &   # Port 50053 - Green Worker
```

### Computer 2 (Person 2)
```bash
# Terminal 1 - C++ Green Team Leader
cd build
./server_b &   # Port 50052

# Terminal 2 - Python Pink Team Leader
cd ..  # Back to mini2 root
source .venv/bin/activate
python src/server_d.py &  # Port 50054
```

### Computer 3 (Person 3)
```bash
cd build
./server_e &                # Port 50055 - Pink Worker (TBD)
python3 src/server_f.py &   # Port 50056 - Pink Worker (TBD)
```

## Testing Person 1 Implementation
```bash
# Run the test script
./test_person1.sh

# Or manually:
cd build
./server_c &  # Start Green Worker
./server_a &  # Start Leader
./client localhost:50051 "test_request"  # Test request
```

# Computer 3
./server_e &
python3 src/server_f.py &
```

## Testing
```bash
# Test locally first
./client localhost:50051 "test_query"

# Test across network
./client 192.168.1.101:50051 "test_query"
```

## Development Tasks
See the todo list for current tasks and assignments.

## Key Requirements
- Chunked data transfer
- Request fairness between teams
- Cancellation support
- Shared memory caching (future)
- Python server implementation