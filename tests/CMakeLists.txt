cmake_minimum_required(VERSION 3.8)

project(distft_tests C CXX)

include(CTest)
enable_testing()

# SPDLOG DEPENDENCY
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/spdlog.cmake)

# Compile test program
set (SOURCES router.cpp session.cpp main.cpp)
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
target_link_libraries(distft_tests 
PRIVATE 
  distft_dht
  distft_utils
  spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)

# add tests
set (TESTS 
      router-insert few-session-create many-session-create 
      session2-store1 session2-store3 
      session10-store0 session10-store1 session10-store2 session10-store3
      session50-store1 session50-store2
      session250-store2 session250-store3
      churn-10-50-1)

foreach(test IN LISTS TESTS)
  add_test(NAME ${test} COMMAND distft_tests ${test})
endforeach()
