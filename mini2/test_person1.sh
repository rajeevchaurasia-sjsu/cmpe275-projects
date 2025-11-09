#!/bin/bash

# Test script for Person 1 (Server A and Server C)
echo "Testing Person 1 servers..."

# Start Server C (Green Worker) in background
echo "Starting Server C (Green Worker)..."
./server_c &
SERVER_C_PID=$!

# Wait a moment for server to start
sleep 2

# Start Server A (Leader) in background
echo "Starting Server A (Leader)..."
./server_a &
SERVER_A_PID=$!

# Wait for servers to initialize
sleep 3

# Test client request
echo "Testing client request..."
./client localhost:50051 "test_request"

# Cleanup
echo "Stopping servers..."
kill $SERVER_A_PID $SERVER_C_PID 2>/dev/null

echo "Test completed!"