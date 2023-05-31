#include "tests.h"

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::debug);
  std::unordered_map<std::string, std::function<bool()>> tests = {
    {"router-insert", test_router_insert},
    {"few-session-create", test_create_few_sessions},
    {"many-session-create", test_create_many_sessions},
    {"session2-store1", store_chunks_fn(1, 2, 3)},
    {"session2-store3", store_chunks_fn(3, 2, 3)},
    {"session10-store0", store_chunks_fn(1, 10, 3)},
    {"session10-store1", store_chunks_fn(10, 10, 10)},
    {"session10-store2", store_chunks_fn(100, 10, 10)},
    {"session10-store3", store_chunks_fn(1000, 10, 100)},
    {"session50-store0", store_chunks_fn(1, 50, 5)},
    {"session50-store1", store_chunks_fn(10, 50, 30)},
    {"session50-store2", store_chunks_fn(100, 50, 30)},
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