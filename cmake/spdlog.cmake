cmake_minimum_required(VERSION 3.8)

find_package(spdlog QUIET)

if(spdlog_FOUND)
  message(STATUS "spdlog found using find_package")
  find_package(spdlog REQUIRED)
else()
  message(STATUS "spdlog not found using find_package, adding subdirectory")
  set(SPDLOG_DIR ${CMAKE_CURRENT_LIST_DIR}/../dependencies/spdlog)
  add_subdirectory(${SPDLOG_DIR} ${CMAKE_CURRENT_BINARY_DIR}/spdlog)
endif()
