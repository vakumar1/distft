#include "tests.h"

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::debug);
  std::unordered_map<std::string, std::function<bool()>> tests = {
    {"router-insert", test_router_insert},
    {"few-session-create", test_create_few_sessions},
    {"many-session-create", test_create_many_sessions},
    {"few-session-store1", test_store_few_sessions1},
    {"few-session-store2", test_store_few_sessions2},
    {"many-session-store1", test_store_many_sessions1},
    {"many-session-store2", test_store_many_sessions2},
    {"many-session-store3", test_store_many_sessions3},
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