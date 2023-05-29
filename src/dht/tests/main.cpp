#include "tests.h"

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::debug);
  std::unordered_map<std::string, std::function<void()>> tests = {
    {"router-insert", test_router_insert},
    {"few-session-create", test_few_sessions},
    {"many-session-create", test_many_sessions},
  };

  if (argc != 2 || tests.count(std::string(argv[1])) == 0) {
    printf("Select exactly one of the following tests to run:\n");
    for (const auto& pair : tests) {
      printf("%s\n", pair.first.c_str());
    }
    return 1;
  }
  tests[std::string(argv[1])]();
  return 0;
}