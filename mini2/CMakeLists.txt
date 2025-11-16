# Minimum version of CMake required
cmake_minimum_required(VERSION 3.10)

# Project name
project(Mini2DataService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to find OpenMP properly
find_package(OpenMP QUIET)

# If OpenMP is not found, try alternative detection for macOS
if(NOT OpenMP_CXX_FOUND AND APPLE)
    # Try to find Homebrew OpenMP
    find_path(OpenMP_INCLUDE_DIR NAMES omp.h
        PATHS /opt/homebrew/opt/libomp/include
              /usr/local/opt/libomp/include
              /opt/local/include/libomp
    )

    find_library(OpenMP_LIBRARY NAMES omp
        PATHS /opt/homebrew/opt/libomp/lib
              /usr/local/opt/libomp/lib
              /opt/local/lib
    )

    if(OpenMP_INCLUDE_DIR AND OpenMP_LIBRARY)
        # Create OpenMP target
        add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
        set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
            INTERFACE_COMPILE_OPTIONS "-Xpreprocessor;-fopenmp;-I${OpenMP_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${OpenMP_LIBRARY}"
        )
        set(OpenMP_CXX_FOUND TRUE)
        message(STATUS "OpenMP found via manual detection: ${OpenMP_LIBRARY}")
    endif()
endif()

# Final check - OpenMP is preferred but not strictly required
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found - enabling parallel processing")
    set(USE_OPENMP TRUE)
else()
    message(WARNING "OpenMP not found - falling back to sequential processing")
    message(WARNING "For better performance, install OpenMP: brew install libomp")
    set(USE_OPENMP FALSE)
endif()

# Help CMake find Homebrew packages on macOS
if(APPLE)
    execute_process(COMMAND brew --prefix grpc
                    OUTPUT_VARIABLE GRPC_PREFIX
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND brew --prefix protobuf
                    OUTPUT_VARIABLE PROTOBUF_PREFIX
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    list(APPEND CMAKE_PREFIX_PATH "${GRPC_PREFIX}" "${PROTOBUF_PREFIX}")
    message(STATUS "Added Homebrew paths: ${GRPC_PREFIX}, ${PROTOBUF_PREFIX}")
endif()

# Find gRPC and Protobuf
find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)

# Find protoc compiler and grpc plugin
find_program(_PROTOBUF_PROTOC protoc REQUIRED)
find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

message(STATUS "Found protoc: ${_PROTOBUF_PROTOC}")
message(STATUS "Found grpc_cpp_plugin: ${_GRPC_CPP_PLUGIN_EXECUTABLE}")

include_directories(${gRPC_INCLUDE_DIRS})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
    
# location of .proto file
set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/protos/dataserver.proto
)

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# command to generate C++ code from the .proto file
add_custom_command(
    OUTPUT
        ${GENERATED_DIR}/dataserver.pb.h
        ${GENERATED_DIR}/dataserver.pb.cc
        ${GENERATED_DIR}/dataserver.grpc.pb.h
        ${GENERATED_DIR}/dataserver.grpc.pb.cc
    COMMAND
        ${_PROTOBUF_PROTOC}
        --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/protos
        --cpp_out=${GENERATED_DIR}
        --grpc_out=${GENERATED_DIR}
        --plugin=protoc-gen-grpc=${_GRPC_CPP_PLUGIN_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/protos/dataserver.proto
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/protos/dataserver.proto
    COMMENT "Generating C++ sources from dataserver.proto"
)

# Creating a library from the generated files.
add_library(protos_lib
    ${GENERATED_DIR}/dataserver.pb.cc
    ${GENERATED_DIR}/dataserver.grpc.pb.cc
)

add_dependencies(protos_lib generate_protos)

add_custom_target(generate_protos DEPENDS
    ${GENERATED_DIR}/dataserver.pb.h
    ${GENERATED_DIR}/dataserver.grpc.pb.h
)

target_link_libraries(protos_lib PUBLIC gRPC::grpc++ protobuf::libprotobuf)

# Executables
add_executable(server_a src/server_a.cpp)
target_link_libraries(server_a protos_lib gRPC::grpc++ protobuf::libprotobuf)

add_executable(server_b src/server_b.cpp)
target_link_libraries(server_b protos_lib gRPC::grpc++ protobuf::libprotobuf)

add_executable(server_c src/server_c.cpp src/AirQualityDataManager.cpp utils/CSVParser.cpp)
target_link_libraries(server_c protos_lib gRPC::grpc++ protobuf::libprotobuf)
if(USE_OPENMP)
    target_link_libraries(server_c OpenMP::OpenMP_CXX)
    target_compile_definitions(server_c PRIVATE USE_OPENMP)
endif()

add_executable(server_e src/server_e.cpp src/AirQualityDataManager.cpp utils/CSVParser.cpp)
target_link_libraries(server_e protos_lib gRPC::grpc++ protobuf::libprotobuf)
if(USE_OPENMP)
    target_link_libraries(server_e OpenMP::OpenMP_CXX)
    target_compile_definitions(server_e PRIVATE USE_OPENMP)
endif()

add_executable(client src/client.cpp)
target_link_libraries(client protos_lib gRPC::grpc++ protobuf::libprotobuf)