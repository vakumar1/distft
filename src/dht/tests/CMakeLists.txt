cmake_minimum_required(VERSION 3.8)

project(DHT_tests C CXX)

# Compile test program
include_directories(${CMAKE_SOURCE_DIR}/..)
set (SOURCES router.cpp session.cpp main.cpp)
set (HEADERS tests.h)
add_executable(dht_tests ${SOURCES} ${HEADERS})
target_link_libraries(dht_tests PUBLIC dht_lib)

set (TESTS router-insert few-session-create many-session-create 
      session2-store1 session2-store3 
      session10-store0 session10-store1 session10-store2 session10-store3
      session50-store1 session50-store2
      session250-store2 session250-store3)

foreach(test IN LISTS TESTS)
  add_test(NAME ${test} COMMAND dht_tests ${test})
endforeach()


