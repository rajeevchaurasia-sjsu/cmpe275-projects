# Mini 2 - Multi-Process Data Service

## Implementation Status
- ✅ **Server A (Leader)**: Implemented - routes requests to team leaders B and D
- ✅ **Server C (Green Worker)**: Implemented - serves sample Green team data
- 🔄 **Server B (Green Team Leader)**: In progress
- 🔄 **Server D (Pink Team Leader)**: In progress
- 🔄 **Server E (Pink Worker)**: In progress
- 🔄 **Server F (Python Pink Worker)**: In progress
- 🔄 **Client**: Basic implementation available

## Team Setup
- **Person 1 (Computer 1)**: Server A (Leader), Server C (Green Worker) ✅
- **Person 2 (Computer 2)**: Server B (Green Team Leader), Server D (Pink Team Leader)
- **Person 3 (Computer 3)**: Server E (Pink Worker), Server F (Python Pink Worker)

## Network Configuration
1. Connect laptops via Ethernet cable
2. Configure static IPs in the 192.168.1.x range
3. Update `network_config.txt` with actual IPs
4. Each computer runs 2 server processes

## Build Instructions
```bash
cd build
cmake ..
make
```

## Running Servers
```bash
# Computer 1
./server_a &
./server_c &

# Computer 2
./server_b &
./server_d &

# Computer 3
./server_e &
python3 src/server_f.py &
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