cmake_minimum_required(VERSION 3.10)
project(WorldBankPopulation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

set(LIBOMP_PREFIX "/opt/homebrew/opt/libomp")
    
if(EXISTS ${LIBOMP_PREFIX})
    message(STATUS "Found libomp at: ${LIBOMP_PREFIX}")
    
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
    set(OpenMP_CXX_INCLUDE_DIRS "${LIBOMP_PREFIX}/include")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "${LIBOMP_PREFIX}/lib/libomp.dylib")
    
    include_directories(${LIBOMP_PREFIX}/include)
    link_directories(${LIBOMP_PREFIX}/lib)
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(OPENMP_LIBRARIES "${LIBOMP_PREFIX}/lib/libomp.dylib")
    
    set(OpenMP_FOUND TRUE)
    message(STATUS "OpenMP enabled with flags: ${OpenMP_CXX_FLAGS}")
else()
    message(WARNING "libomp not found at ${LIBOMP_PREFIX}")
    set(OpenMP_FOUND FALSE)
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/../utils)
include_directories(${PROJECT_SOURCE_DIR}/commons)

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

# Threading test
add_executable(threading_test
    tests/test_threading.cpp
    PopulationDataManagerHash.cpp
    commons/WorldBankCSVLoader.cpp
    ../utils/CSVParser.cpp
    ../utils/BenchmarkTimer.cpp
)

# Link OpenMP if found
if(OpenMP_FOUND)
    target_link_libraries(population_test ${OPENMP_LIBRARIES})
    target_link_libraries(population_compare ${OPENMP_LIBRARIES})
    target_link_libraries(threading_test ${OPENMP_LIBRARIES})
    message(STATUS "Linked OpenMP to all targets")
else()
    message(WARNING "OpenMP not available - threading will be disabled")
endif()