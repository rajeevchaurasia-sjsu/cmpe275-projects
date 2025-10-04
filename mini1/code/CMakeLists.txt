cmake_minimum_required(VERSION 3.10)

# Project name
project(AirQualityAnalysis)

# Set C++ standard to C++17 (needed for filesystem)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Add executable for main test program
add_executable(air_quality_test
    main.cpp
    AirQualityDataManager.cpp
    utils/CSVParser.cpp
    utils/BenchmarkTimer.cpp
)