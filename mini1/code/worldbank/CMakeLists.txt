cmake_minimum_required(VERSION 3.10)
project(WorldBankPopulation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/../utils)
include_directories(${PROJECT_SOURCE_DIR}/../commons)

# Original test (vector version)
add_executable(population_test
    tests/test_basic.cpp
    PopulationDataManager.cpp
    commons/WorldBankCSVLoader.cpp
    ../utils/CSVParser.cpp
    ../utils/BenchmarkTimer.cpp
)

# Comparison test (vector vs map)
add_executable(population_compare
    tests/test_comparison.cpp
    PopulationDataManager.cpp
    PopulationDataManagerMap.cpp
    PopulationDataManagerHash.cpp
    commons/WorldBankCSVLoader.cpp
    ../utils/CSVParser.cpp
    ../utils/BenchmarkTimer.cpp
)