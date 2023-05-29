#include "tests.h"

void test_few_sessions() {
  unsigned int num_endpoints = 2;
  assert(create_destroy_sessions(num_endpoints));
}

void test_many_sessions() {
  unsigned int num_endpoints = 10;
  assert(create_destroy_sessions(num_endpoints));
}

// create a destroy sessions (makes sure that all are successfully created without timing out)
bool create_destroy_sessions(unsigned int num_endpoints) {
  auto create_session = [num_endpoints](int i) {
    unsigned int my_port = i;
    unsigned int other_port = (i + 1) % num_endpoints;
    char my_addr[20];
    char other_addr[20];
    sprintf(my_addr, "localhost:3003%i", my_port);
    sprintf(other_addr, "localhost:3003%i", other_port);
    Session* s = new Session(std::string(my_addr), std::string(other_addr));
    std::this_thread::sleep_for(std::chrono::seconds(3));
  };
  std::vector<std::thread*> threads;
  for (int i = 1; i < num_endpoints; i++) {
    std::thread* session_thread = new std::thread(create_session, i);
    threads.push_back(session_thread);
  }
  create_session(0);
  while (threads.size() > 0) {
    std::thread* session_thread = threads.back();
    threads.pop_back();
    session_thread->join();
    delete session_thread;
  }
  return true;
}