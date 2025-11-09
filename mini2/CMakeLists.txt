# Minimum version of CMake required
cmake_minimum_required(VERSION 3.10)

# Project name
project(Mini2DataService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)

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
        ${gRPC_PROTOC_EXECUTABLE}
        --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/protos
        --cpp_out=${GENERATED_DIR}
        --grpc_out=${GENERATED_DIR}
        --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE}
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

target_link_libraries(protos_lib PUBLIC gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)

# Executables
add_executable(server_a src/server_a.cpp)
target_link_libraries(server_a protos_lib gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)

add_executable(server_b src/server_b.cpp)
target_link_libraries(server_b protos_lib gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)

add_executable(server_c src/server_c.cpp)
target_link_libraries(server_c protos_lib gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)

add_executable(server_d src/server_d.cpp)
target_link_libraries(server_d protos_lib gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)

add_executable(server_e src/server_e.cpp)
target_link_libraries(server_e protos_lib gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)

add_executable(client src/client.cpp)
target_link_libraries(client protos_lib gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)