#include "tests/tests.h"

#include <spdlog/spdlog.h>
#include <string>

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::debug);
  std::unordered_map<std::string, std::function<bool()>> tests = {
    // DHT tests
    {"few-session-create", create_destroy_sessions(3)},
    {"many-session-create", create_destroy_sessions(20)},
    {"session-store-2-1", store_chunks_fn(1, 2)},
    {"session-store-2-3", store_chunks_fn(3, 2)},
    {"session-store-10-0", store_chunks_fn(1, 10)},
    {"session-store-10-1", store_chunks_fn(10, 10)},
    {"session-store-10-2", store_chunks_fn(100, 10)},
    {"session-store-10-3", store_chunks_fn(1000, 10)},
    {"session-store-50-0", store_chunks_fn(1, 50)},
    {"session-store-50-1", store_chunks_fn(10, 50)},
    {"session-store-50-2", store_chunks_fn(100, 50)},
    {"session-store-50-3", store_chunks_fn(1000, 50)},
    {"session-store-250-2", store_chunks_fn(100, 250)},
    {"session-store-250-3", store_chunks_fn(1000, 250)},
    {"churn-10-50-1", churn_chunks_fn(1, 10, 50, 10)},
    {"churn-5-50-5", churn_chunks_fn(5, 5, 50, 10)},
    {"churn-10-200-1", churn_chunks_fn(1, 10, 200, 10)},

    // file tests
    {"server-only-static-10-10-100", server_static_files(10, 10, 100, 0, 0)},
    {"server-only-static-50-10-100", server_static_files(50, 10, 100, 0, 0)},
    {"server-only-dynamic-10-10-100", server_dynamic_files(10, 10, 100, 0, 0)},
    {"server-only-dynamic-50-10-100", server_dynamic_files(50, 10, 100, 0, 0)},
    {"server-only-dynamic-50-100-100", server_dynamic_files(50, 100, 100, 3, 3)},
    {"server-only-dynamic-100-100-100", server_dynamic_files(100, 100, 100, 3, 3)},
  };

  if (argc != 2 || tests.count(std::string(argv[1])) == 0) {
    printf("Select exactly one of the following tests to run:\n");
    for (const auto& pair : tests) {
      printf("%s\n", pair.first.c_str());
    }
    return 1;
  }
  bool correct = tests[std::string(argv[1])]();
  printf("Test %s\n", correct ? "passed" : "failed");
  return correct ? 0 : 1;
}