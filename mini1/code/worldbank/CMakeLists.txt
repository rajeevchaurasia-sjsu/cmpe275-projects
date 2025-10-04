cmake_minimum_required(VERSION 3.10)
project(WorldBankPopulation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/../utils)

add_executable(population_test
    tests/test_basic.cpp
    ../utils/CSVParser.cpp
    ../utils/BenchmarkTimer.cpp
    PopulationDataManager.cpp
)