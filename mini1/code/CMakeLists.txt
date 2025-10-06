cmake_minimum_required(VERSION 3.10)

# Project name
project(AirQualityAnalysis)

# Set C++ standard to C++17 (needed for filesystem)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Find OpenMP (macOS specific setup)
if(APPLE)
    # For macOS with Homebrew libomp
    set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp")
    set(OpenMP_C_LIB_NAMES "omp")
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
    
    include_directories("/opt/homebrew/opt/libomp/include")
    link_directories("/opt/homebrew/opt/libomp/lib")
endif()

find_package(OpenMP REQUIRED)

# Add executable for main test program
add_executable(air_quality_test
    main.cpp
    AirQualityDataManager.cpp
    utils/CSVParser.cpp
    utils/BenchmarkTimer.cpp
)

# Link OpenMP
if(OpenMP_CXX_FOUND)
    target_link_libraries(air_quality_test OpenMP::OpenMP_CXX)
endif()

# Add parallel benchmark executable
add_executable(parallel_benchmark
    parallel_benchmark.cpp
    AirQualityDataManager.cpp
    utils/CSVParser.cpp
    utils/BenchmarkTimer.cpp
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(parallel_benchmark OpenMP::OpenMP_CXX)
endif()