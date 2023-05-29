cmake_minimum_required(VERSION 3.8)

project(DHT C CXX)

# GRPC DEPENDENCY
include(common.cmake)

# Proto file
get_filename_component(dht_proto "./dht.proto" ABSOLUTE)
get_filename_component(dht_proto_path "${dht_proto}" PATH)

# Generated sources
set(dht_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/dht.pb.cc")
set(dht_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/dht.pb.h")
set(dht_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/dht.grpc.pb.cc")
set(dht_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/dht.grpc.pb.h")
add_custom_command(
      OUTPUT "${dht_proto_srcs}" "${dht_proto_hdrs}" "${dht_grpc_srcs}" "${dht_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${dht_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${dht_proto}"
      DEPENDS "${dht_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# dht_grpc_proto
add_library(dht_grpc_proto
  ${dht_grpc_srcs}
  ${dht_grpc_hdrs}
  ${dht_proto_srcs}
  ${dht_proto_hdrs})
target_link_libraries(dht_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# SPDLOG DEPENDENCY
add_subdirectory("../../dependencies/spdlog" build)

# COMPILING EXECUTABLES + LIBS
# Compile main program
set (CMAKE_CXX_FLAGS "-g")
set (SOURCES key.cpp router.cpp rpc.cpp session.cpp)
set (MAIN_SOURCE main.cpp)
set (HEADERS key.h router.h session.h)
add_executable(dht_server ${SOURCES} ${MAIN_SOURCE} ${HEADERS})
target_link_libraries(dht_server
  dht_grpc_proto
  absl::flags
  absl::flags_parse
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF}
  spdlog::spdlog_header_only)

# Compile client
set (CLIENT_SOURCES key.cpp test_client.cpp)
set (CLIENT_HEADERS key.h)
add_executable(dht_client ${CLIENT_SOURCES} ${CLIENT_HEADERS})
target_link_libraries(dht_client
  dht_grpc_proto
  absl::flags
  absl::flags_parse
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF}
  spdlog::spdlog_header_only)

# Compile library
add_library(dht_lib ${SOURCES} ${HEADERS})
target_link_libraries(dht_lib
  dht_grpc_proto
  absl::flags
  absl::flags_parse
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF}
  spdlog::spdlog_header_only)

# Tests
include(CTest)
enable_testing()
add_subdirectory(tests tests/build)

