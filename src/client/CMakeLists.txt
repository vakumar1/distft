cmake_minimum_required(VERSION 3.8)

project(distft_client C CXX)

set (DISTFT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../)

# SPDLOG DEPENDENCY
include(${DISTFT_ROOT}/cmake/spdlog.cmake)

# Compile test program
set (SOURCES main.cpp)
set (HEADERS )
add_executable(distft_client ${SOURCES} ${HEADERS})

# Link dependencies
set(SRC_DIR ${DISTFT_ROOT}/src)
if (NOT TARGET distft_utils)
  add_subdirectory(${DISTFT_ROOT}/src/utils ${CMAKE_CURRENT_BINARY_DIR}/utils)
endif()
if (NOT TARGET distft_dht)
  add_subdirectory(${DISTFT_ROOT}/src/dht ${CMAKE_CURRENT_BINARY_DIR}/dht)
endif()
target_link_libraries(distft_client 
PRIVATE 
  distft_dht
  distft_utils
  spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)
