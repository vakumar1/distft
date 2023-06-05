cmake_minimum_required(VERSION 3.8)

project(distft_utils C CXX)

set (SOURCES utils.cpp)
set (HEADERS utils.h)
add_library(distft_utils ${SOURCES} ${HEADERS})
target_include_directories(distft_utils
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
