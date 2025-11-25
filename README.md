# CMPE 275 Projects: Distributed Data Systems

This repository contains the source code and documentation for two distinct research projects focusing on high-performance computing, parallelization, and distributed systems.

## Table of Contents
- [Mini 1: Threading and Memory Observations](#mini-1-threading-and-memory-observations)
- [Mini 2: Multi-Process Air Quality Data Service](#mini-2-multi-process-air-quality-data-service)

---

# Mini 1: Threading and Memory Observations

**Focus:** Threading and memory observations and relationships.
**Version:** 0.1

The goal of this research project is to investigate how parallelization and data structures interact. It is not a functional (app-based) driven outcome, but rather an observation of how design affects memory and the use of resources. All work is performed within a single process to better utilize debugging tools.

### 📂 Data Sets
The following data sets are used for assessing design and investigations:
1.  **2020 Fire Data**: Air quality and fire incident records.
2.  **World Bank Population Data**: Global population statistics.

### 🛠️ Technology Stack
* **Language**: C++ (C++17 Standard)
* **Parallelization**: OpenMP (OMP) / pthreads
* **Build System**: CMake
* **Tools**: Linters, Valgrind, Perf (optional)

### 🏗️ Build Instructions

1.  **Navigate to the code directory:**
    ```bash
    cd mini1/code/2020-fire
    # or
    cd mini1/code/worldbank
    ```

2.  **Build using CMake:**
    ```bash
    mkdir build
    cd build
    cmake ..
    make
    ```

### 🏃 Running the Benchmarks

The build process generates executables for testing correctness and benchmarking parallel performance.

**Run the basic correctness test:**
```bash
./air_quality_test
```

**Run the parallel benchmark:**
```bash
./parallel_benchmark
```

---

# Mini 2: Multi-Process Air Quality Data Service

**Goal:** Architect a scalable, distributed data retrieval system using a **Leader-Worker** model. This project demonstrates inter-process communication between heterogeneous nodes (C++ and Python) using **gRPC** and **Protocol Buffers**.

### 🚀 Key Features
* **Hybrid Tech Stack**: Interoperability between C++ and Python servers.
* **Chunked Streaming**: Data is transferred in `DataChunk` messages to ensure network fairness and responsiveness.
* **Cancellation**: Support for cancelling ongoing data streams.
* **Leader-Worker Architecture**: Distributed role-based processing across multiple physical machines.

### 🏛️ Team Setup & Architecture

The system is designed to run on 3 physical computers connected via Ethernet with static IPs (192.168.1.x).

| Computer | Person | Server Roles | Port |
| :--- | :--- | :--- | :--- |
| **Computer 1** | Person 1 | **Server A (Leader)** <br> **Server C (Green Worker)** | `50051`<br>`50053` |
| **Computer 2** | Person 2 | **Server B (Green Team Leader)** <br> **Server D (Pink Team Leader)** | `50052`<br>`50054` |
| **Computer 3** | Person 3 | **Server E (Pink Worker)** <br> **Server F (Pink Worker)** | `50055`<br>`50056` |

### 📦 Prerequisites

#### For C++ Servers (A, B, C, E)
```bash
# macOS
brew install grpc protobuf cmake

# Linux
sudo apt-get install -y build-essential cmake libgrpc++-dev protobuf-compiler-grpc
```

#### For Python Servers (D, F)
```bash
cd mini2
python3 -m venv .venv
source .venv/bin/activate  # Windows: .venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt

# Generate Python Protobuf code
python -m grpc_tools.protoc \
  -I./protos \
  --python_out=./src/python \
  --grpc_python_out=./src/python \
  ./protos/dataserver.proto
```

### 🏗️ Build Instructions (C++)

```bash
cd mini2/build
cmake ..
make -j4
```

### 🖥️ Running the System

**Computer 1 (Person 1):**
```bash
cd build
./server_a &   # Leader
./server_c &   # Green Worker
```

**Computer 2 (Person 2):**
```bash
cd build
./server_b &   # Green Team Leader

# In a separate terminal for Python:
cd ../src/python
python3 server_d.py &  # Pink Team Leader
```

**Computer 3 (Person 3):**
```bash
cd build
./server_e &   # Pink Worker (C++)

# In a separate terminal for Python:
cd ../src/python
python3 server_f.py &  # Pink Worker (Python)
```

### 🎮 Client Usage

The client requests data from the Leader (Server A).

**Syntax:**
```bash
./client <Leader_IP>:<Port> "<Query_String>"
```

**Example (Local Test):**
```bash
./client localhost:50051 "green_data"
```

**Example (Network Test):**
```bash
./client 192.168.1.101:50051 "test_query"
```
