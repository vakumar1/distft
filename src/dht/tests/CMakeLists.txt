cmake_minimum_required(VERSION 3.8)

project(DHT_tests C CXX)

# include(ExternalProject)

# ExternalProject_Add(
#   DHT
#   SOURCE_DIR ${CMAKE_SOURCE_DIR}/../../dht
#   BINARY_DIR ${CMAKE_BINARY_DIR}/../../dht/build
#   BUILD_ALWAYS 1
#   INSTALL_COMMAND ""
# )

# Compile main program
include_directories(${CMAKE_SOURCE_DIR}/..)
set (SOURCES router.cpp session.cpp main.cpp)
set (HEADERS tests.h)
add_executable(dht_tests ${SOURCES} ${HEADERS})
target_link_libraries(dht_tests PUBLIC dht_lib)
add_test(NAME TestRouter COMMAND dht_tests)


