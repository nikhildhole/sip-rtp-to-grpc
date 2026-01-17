cmake_minimum_required(VERSION 3.15)
project(sip_rtp_gateway CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Threads REQUIRED)

if(NOT gRPC_CPP_PLUGIN_EXECUTABLE)
    find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
endif()

# Proto generation
set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto_gen)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILES ${PROTO_SRC_DIR}/voicebot.proto)

# Helper to generate protobuf/grpc sources
add_custom_command(
    OUTPUT ${PROTO_GEN_DIR}/voicebot.pb.cc ${PROTO_GEN_DIR}/voicebot.pb.h
           ${PROTO_GEN_DIR}/voicebot.grpc.pb.cc ${PROTO_GEN_DIR}/voicebot.grpc.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out=${PROTO_GEN_DIR}
         --cpp_out=${PROTO_GEN_DIR}
         -I ${PROTO_SRC_DIR}
         --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE}
         ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
)

# Source files
file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
    "src/*.h"
)

# Include directories
include_directories(
    src
    ${PROTO_GEN_DIR}
)

# Executable
add_executable(sip_rtp_gateway
    ${SRC_FILES}
    ${PROTO_GEN_DIR}/voicebot.pb.cc
    ${PROTO_GEN_DIR}/voicebot.grpc.pb.cc
)

    target_link_libraries(sip_rtp_gateway
    PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    yaml-cpp::yaml-cpp
    Threads::Threads
)

# Compile options
if (MSVC)
    target_compile_options(sip_rtp_gateway PRIVATE /W3 /wd4005 /wd4244 /wd4267)
else()
    target_compile_options(sip_rtp_gateway PRIVATE -Wall -Wextra -Wno-unused-parameter -pthread)
endif()
