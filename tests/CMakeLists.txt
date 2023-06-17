cmake_minimum_required(VERSION 3.8)

project(distft_tests C CXX)

include(CTest)
enable_testing()

# SPDLOG DEPENDENCY
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/spdlog.cmake)

# Compile test program
set (CMAKE_CXX_FLAGS "-g")
set (SOURCES utils.cpp session.cpp file.cpp main.cpp)
set (HEADERS tests.h)
add_executable(distft_tests ${SOURCES} ${HEADERS})

# Link dependencies
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if (NOT TARGET distft_utils)
  add_subdirectory(${SRC_DIR}/utils ${CMAKE_CURRENT_BINARY_DIR}/utils)
endif()
if (NOT TARGET distft_dht)
  add_subdirectory(${SRC_DIR}/dht ${CMAKE_CURRENT_BINARY_DIR}/dht)
endif()
if (NOT TARGET distft_file)
  add_subdirectory(${SRC_DIR}/client ${CMAKE_CURRENT_BINARY_DIR}/client)
endif()
if (NOT TARGET distft_client)
  add_subdirectory(${SRC_DIR}/client ${CMAKE_CURRENT_BINARY_DIR}/client)
endif()
target_link_libraries(distft_tests 
PRIVATE
  distft_client
  distft_file
  distft_dht
  distft_utils
  spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)

# add tests
set (TESTS 
      # session tests
      few-session-create many-session-create 
      session-store-2-1 session-store-2-3 
      session-store-10-0 session-store-10-1 session-store-10-2 session-store-10-3
      session-store-50-1 session-store-50-2
      session-store-250-2 session-store-250-3
      churn-10-50-1 churn-5-50-5
      
      # file tests
      server-only-static-10-10-100 server-only-static-50-10-100
      server-only-dynamic-10-10-100 server-only-dynamic-50-10-100  
)

foreach(test IN LISTS TESTS)
  add_test(NAME ${test} COMMAND distft_tests ${test})
endforeach()
