#include "tests.h"

bool test_create_few_sessions() {
  unsigned int num_endpoints = 2;
  return create_destroy_sessions(num_endpoints);
}

bool test_create_many_sessions() {
  unsigned int num_endpoints = 10;
  return create_destroy_sessions(num_endpoints);
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

// returns a function that creates/destroys sessions and stores/gets chunks
std::function<bool()> store_chunks_fn(unsigned int num_chunks, unsigned int num_endpoints, unsigned int wait_time) {
  auto fn = [num_chunks, num_endpoints, wait_time]() {
    std::mutex correct_lock;
    unsigned int num_correct = 0;
    Session* sessions[num_endpoints];
    Chunk* chunks[num_chunks];

    auto get_session = [&correct_lock, &num_correct, &sessions, &chunks, num_chunks, num_endpoints, wait_time](int i) {
      // create session
      sessions[i] = create_session(num_endpoints, i);
      Session* s = sessions[i];
      std::this_thread::sleep_for(std::chrono::seconds(wait_time));

      // get and validate
      for (int j = 0; j < num_chunks; j++) {
        Chunk* c = chunks[j];
        char* data_buff;
        size_t size_buff;
        bool found = s->get(c->key, &data_buff, &size_buff);
        if (!found) {
          spdlog::error("{} CHUNK NOT FOUND: CHUNK={}", hex_string(s->self_key()), hex_string(c->key));
          return;
        }
        if (c->size != size_buff) {
          spdlog::error("{} INCORRECT SIZE: CHUNK={} EXPECTED_SIZE={} ACTUAL_SIZE={}", hex_string(s->self_key()),
                          hex_string(c->key), c->size, size_buff);
          return;
        }
        for (int k = 0; k < size_buff; k++) {
          if (c->data[k] != data_buff[k]) {
            spdlog::error("{} INCORRECT VALUE: CHUNK={} BYTE={} EXPECTED_VALUE={} ACTUAL_VALUE={}", hex_string(s->self_key()), 
                          hex_string(c->key), j, static_cast<uint8_t>(c->data[k]), static_cast<uint8_t>(data_buff[k]));
            return;
          }
        }
        spdlog::debug("{} CHUNK CORRECT: CHUNK={}", hex_string(s->self_key()), hex_string(c->key));
      }
      correct_lock.lock();
      num_correct += 1;
      correct_lock.unlock();
    };

    std::vector<std::thread*> threads;
    for (int i = 1; i < num_endpoints; i++) {
      threads.push_back(new std::thread(get_session, i));
    }
    sessions[0] = create_session(num_endpoints, 0);
    for (int i = 0; i < num_chunks; i++) {
      size_t size = i + 5;
      chunks[i] = random_chunk(size);
      char* data_buffer = new char[size];
      std::memcpy(data_buffer, chunks[i]->data, size);
      sessions[0]->set(data_buffer, size);
    }

    // wait for session to complete
    std::this_thread::sleep_for(std::chrono::seconds(wait_time));
    while (threads.size() > 0) {
      threads.back()->join();
      threads.pop_back();
    }

    // delete chunks and threads
    for (int i = 0; i < num_chunks; i++) {
      delete chunks[i];
    }
    for (int i = 0; i < num_endpoints; i++) {
      threads.push_back(new std::thread([&sessions](int i) {delete sessions[i];}, i));
    }
    while (threads.size() > 0) {
      threads.back()->join();
      threads.pop_back();
    }
    return num_correct == num_endpoints - 1;
  };
  return fn;
}

Chunk* random_chunk(size_t size) {
  char* data_buffer = new char[size];
  for (int i = 0; i < size; i++) {
    data_buffer[i] = static_cast<char>(std::rand() % 0xFF);
  }
  return new Chunk(data_buffer, size, true, std::chrono::steady_clock::now());
}