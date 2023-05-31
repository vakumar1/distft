#include "tests.h"

bool test_create_few_sessions() {
  unsigned int num_endpoints = 2;
  return create_destroy_sessions(num_endpoints);
}

bool test_create_many_sessions() {
  unsigned int num_endpoints = 10;
  return create_destroy_sessions(num_endpoints);
}

bool test_store_few_sessions1() {
  unsigned int num_endpoints = 2;
  unsigned int num_chunks = 1;
  return store_chunks(num_chunks, num_endpoints);
}

bool test_store_few_sessions2() {
  unsigned int num_endpoints = 2;
  unsigned int num_chunks = 3;
  return store_chunks(num_chunks, num_endpoints);
}

bool test_store_many_sessions1() {
  unsigned int num_endpoints = 10;
  unsigned int num_chunks = 1;
  return store_chunks(num_chunks, num_endpoints);
}

bool test_store_many_sessions2() {
  unsigned int num_endpoints = 10;
  unsigned int num_chunks = 3;
  return store_chunks(num_chunks, num_endpoints);
}

bool test_store_many_sessions3() {
  unsigned int num_endpoints = 10;
  unsigned int num_chunks = 20;
  return store_chunks(num_chunks, num_endpoints);
}

// HELPERS

Session* create_session(unsigned int num_endpoints, unsigned int i) {
  unsigned int my_port = i;
  unsigned int other_port = (i + 1) % num_endpoints;
  char my_addr[20];
  char other_addr[20];
  sprintf(my_addr, "localhost:%i", 30030 + my_port);
  sprintf(other_addr, "localhost:%i", 30030 + other_port);
  Session* s = new Session(std::string(my_addr), std::string(other_addr));
  std::this_thread::sleep_for(std::chrono::seconds(3));
  return s;
}

// create and destroy sessions (makes sure that all are successfully created without timing out)
bool create_destroy_sessions(unsigned int num_endpoints) {
  
  std::vector<std::thread*> threads;
  for (int i = 1; i < num_endpoints; i++) {
    std::thread* session_thread = new std::thread(create_session, num_endpoints, i);
    threads.push_back(session_thread);
  }
  create_session(num_endpoints, 0);
  while (threads.size() > 0) {
    std::thread* session_thread = threads.back();
    threads.pop_back();
    session_thread->join();
    delete session_thread;
  }
  return true;
}

bool store_chunks(unsigned int num_chunks, unsigned int num_endpoints) {
  std::mutex correct_lock;
  unsigned int num_correct = 0;
  Chunk* chunks[num_chunks];

  auto get_session = [&correct_lock, &num_correct, &chunks, num_endpoints, num_chunks](int i) {
    // create session
    Session* s = create_session(num_endpoints, i);
    std::this_thread::sleep_for(std::chrono::seconds(3 * num_chunks));

    // get and validate
    for (int i = 0; i < num_chunks; i++) {
      std::byte* data_buff;
      size_t size_buff;
      bool found = s->get(chunks[i]->key, &data_buff, &size_buff);
      if (!found) {
        spdlog::debug("{} CHUNK NOT FOUND: CHUNK={}", hex_string(s->self_key()));
        return;
      }
      if (chunks[i]->size != size_buff) {
        spdlog::debug("{} INCORRECT SIZE: CHUNK={} EXPECTED_SIZE={} ACTUAL_SIZE={}", hex_string(s->self_key()),
                        hex_string(chunks[i]->key), chunks[i]->size, size_buff);
        return;
      }
      for (int j = 0; j < size_buff; j++) {
        if (chunks[i]->data[j] != data_buff[j]) {
          spdlog::debug("{} INCORRECT VALUE: CHUNK={} BYTE={} EXPECTED_VALUE={} ACTUAL_VALUE={}", hex_string(s->self_key()), 
                        hex_string(chunks[i]->key), j, static_cast<int>(chunks[i]->data[j]), static_cast<int>(data_buff[j]));
          return;
        }
      }
      spdlog::debug("{} CHUNK CORRECT: CHUNK={}", hex_string(s->self_key()), hex_string(chunks[i]->key));
    }
    correct_lock.lock();
    num_correct += 1;
    correct_lock.unlock();
  };

  std::vector<std::thread*> threads;
  for (int i = 1; i < num_endpoints; i++) {
    std::thread* session_thread = new std::thread(get_session, i);
    threads.push_back(session_thread);
  }
  Session* s = create_session(num_endpoints, 0);
  for (int i = 0; i < num_chunks; i++) {
    chunks[i] = random_chunk(i + 5);
    s->set(chunks[i]->data, i + 5);
  }
  while (threads.size() > 0) {
    std::thread* session_thread = threads.back();
    threads.pop_back();
    session_thread->join();
    delete session_thread;
  }
  return num_correct == num_endpoints - 1;
}

Chunk* random_chunk(size_t size) {
  std::byte* data_buffer = new std::byte[size];
  for (int i = 0; i < size; i++) {
    data_buffer[i] = std::byte(std::rand() % 0xFF);
  }
  return new Chunk(data_buffer, size, true, std::chrono::steady_clock::now());
}