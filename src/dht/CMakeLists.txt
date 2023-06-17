cmake_minimum_required(VERSION 3.8)

project(distft_dht C CXX)

# PROTOBUF + GRPC DEPENDENCY 
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/grpc.cmake)

# SPDLOG DEPENDENCY
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/spdlog.cmake)

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

# dht_grpc_proto lib
add_library(dht_grpc_proto
  ${dht_grpc_srcs}
  ${dht_grpc_hdrs}
  ${dht_proto_srcs}
  ${dht_proto_hdrs})
target_link_libraries(dht_grpc_proto PUBLIC
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})


# COMPILING DHT LIB
set (CMAKE_CXX_FLAGS "-g")
set (SOURCES router.cpp rpc.cpp session.cpp)
set (HEADERS router.h session.h)
add_library(distft_dht ${SOURCES} ${HEADERS})

target_include_directories(distft_dht 
PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

# Link dependencies
if (NOT TARGET distft_utils)
  set(UTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../utils)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../utils ${CMAKE_CURRENT_BINARY_DIR}/utils)
endif()
target_link_libraries(distft_dht 
PUBLIC
  dht_grpc_proto
  distft_utils
  absl::flags
  absl::flags_parse
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF}
  spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)
