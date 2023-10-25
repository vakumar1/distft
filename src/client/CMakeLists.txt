cmake_minimum_required(VERSION 3.8)

project(distft_founder C CXX)

set (DISTFT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../)

# SPDLOG DEPENDENCY
include(${DISTFT_ROOT}/cmake/spdlog.cmake)

# local lib dependencies
if (NOT TARGET distft_utils)
  add_subdirectory(${DISTFT_ROOT}/src/utils ${CMAKE_CURRENT_BINARY_DIR}/utils)
endif()
if (NOT TARGET distft_dht)
  add_subdirectory(${DISTFT_ROOT}/src/dht ${CMAKE_CURRENT_BINARY_DIR}/dht)
endif()

set (CMAKE_CXX_FLAGS "-g")

# Compile file lib
set (SOURCES file.cpp)
set (HEADERS file.h)
add_library(distft_file ${SOURCES} ${HEADERS})
target_link_libraries(distft_file 
PRIVATE 
  distft_dht
  distft_utils
)

# Compile client lib
set (SOURCES cmd.cpp daemon.cpp interactive.cpp)
set (HEADERS client.h)
add_library(distft_client ${SOURCES} ${HEADERS})

# Link dependencies
set(SRC_DIR ${DISTFT_ROOT}/src)
target_link_libraries(distft_client 
PRIVATE
  distft_file
  distft_dht
  distft_utils
  spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)

# Compile test program
add_executable(distft_founder founder.cpp)

# Link dependencies
set(SRC_DIR ${DISTFT_ROOT}/src)
target_link_libraries(distft_founder 
PRIVATE
  distft_client
)
