#include "tests.h"

// HELPER DEFS.
void create_session(unsigned int num_endpoints, unsigned int create_wait, unsigned int i);
bool create_destroy_sessions(unsigned int num_endpoints);
Chunk* random_chunk(size_t size);

bool test_create_few_sessions() {
  unsigned int num_endpoints = 2;
  return create_destroy_sessions(num_endpoints);
}

bool test_create_many_sessions() {
  unsigned int num_endpoints = 10;
  return create_destroy_sessions(num_endpoints);
}

// returns a function that creates/destroys sessions and stores/gets chunks
std::function<bool()> store_chunks_fn(unsigned int num_chunks, unsigned int num_endpoints) {
  auto fn = [num_chunks, num_endpoints]() {
    // track sessions/chunk storage
    Session* sessions[num_endpoints];
    Chunk* chunks[num_chunks];
    std::mutex correct_lock;
    unsigned int num_correct = 0;

    // create a session
    auto create_session = [&sessions, num_endpoints](unsigned int i) {
      unsigned int my_port = i;
      unsigned int other_port = (i + 1) % num_endpoints;
      char my_addr[20];
      char other_addr[20];
      sprintf(my_addr, "localhost:%i", 2000 + my_port);
      sprintf(other_addr, "localhost:%i", 2000 + other_port);
      sessions[i] = new Session;
      sessions[i]->startup(std::string(my_addr), std::string(other_addr));
    };

    // get chunks from session and validate
    auto check_chunks = [&correct_lock, &num_correct, &sessions, &chunks, num_chunks](unsigned int i) {
      Session* s = sessions[i];

      // get and validate
      for (int j = 0; j < num_chunks; j++) {
        Chunk* c = chunks[j];
        char* data_buff;
        size_t size_buff;
        bool found = s->get(c->key, &data_buff, &size_buff);
        if (!found) {
          spdlog::error("{} CHUNK NOT FOUND: CHUNK={}", hex_string(s->self_key()), hex_string(c->key));
          continue;
        }
        if (c->size != size_buff) {
          spdlog::error("{} INCORRECT SIZE: CHUNK={} EXPECTED_SIZE={} ACTUAL_SIZE={}", hex_string(s->self_key()),
                          hex_string(c->key), c->size, size_buff);
          continue;
        }
        bool incorrect = false;
        for (int k = 0; k < size_buff; k++) {
          if (c->data[k] != data_buff[k]) {
            spdlog::error("{} INCORRECT VALUE: CHUNK={} BYTE={} EXPECTED_VALUE={} ACTUAL_VALUE={}", hex_string(s->self_key()), 
                          hex_string(c->key), j, static_cast<uint8_t>(c->data[k]), static_cast<uint8_t>(data_buff[k]));
            incorrect = true;
            break;
          }
        }
        if (incorrect) {
          continue;
        }
        spdlog::debug("{} CHUNK CORRECT: CHUNK={}", hex_string(s->self_key()), hex_string(c->key));
        correct_lock.lock();
        num_correct += 1;
        correct_lock.unlock();
      }
      
    };
    std::vector<std::thread*> threads;

    // create sessions
    for (int i = 1; i < num_endpoints; i++) {
      threads.push_back(new std::thread(create_session, i));
    }
    create_session(0);
    while (threads.size() > 0) {
      threads.back()->join();
      delete threads.back();
      threads.pop_back();
    }

    // set chunks in session and create threads to verify
    for (int i = 0; i < num_chunks; i++) {
      size_t size = i + 5;
      chunks[i] = random_chunk(size);
      char* data_buffer = new char[size];
      std::memcpy(data_buffer, chunks[i]->data, size);
      sessions[0]->set(data_buffer, size);
    }
    for (int i = 1; i < num_endpoints; i++) {
      threads.push_back(new std::thread(check_chunks, i));
    }
    while (threads.size() > 0) {
      threads.back()->join();
      delete threads.back();
      threads.pop_back();
    }

    // delete chunks and sessions
    for (int i = 0; i < num_chunks; i++) {
      delete chunks[i];
    }
    for (int i = 0; i < num_endpoints; i++) {
      threads.push_back(new std::thread([&sessions](int i) {
        sessions[i]->teardown(false);
        delete sessions[i];
      }, i));
    }
    while (threads.size() > 0) {
      threads.back()->join();
      delete threads.back();
      threads.pop_back();
    }
    return num_correct >= ((num_endpoints - 1) * num_chunks);
  };
  return fn;
}

std::function<bool()> churn_chunks_fn(unsigned int num_chunks, unsigned int num_servers, unsigned int num_clients) {
  num_clients = num_clients / 10;
  auto fn = [num_chunks, num_servers, num_clients]() {
    // track sessions/chunk storage
    Session* servers[num_servers];
    Session* clients[10];
    Chunk* chunks[(num_servers + num_clients) * num_chunks];
    std::mutex correct_lock;
    unsigned int num_correct = 0;

    // create a session
    auto create_session = [](Session** session_buffer, unsigned int i, unsigned int num_endpoints) {
      unsigned int my_port = i;
      unsigned int other_port = (i + 1) % num_endpoints;
      char my_addr[20];
      char other_addr[20];
      sprintf(my_addr, "localhost:%i", 2000 + my_port);
      sprintf(other_addr, "localhost:%i", 2000 + other_port);
      Session* s = new Session;
      s->startup(std::string(my_addr), std::string(other_addr));
      *session_buffer = s;
    };

    auto create_chunk = [](Session* s, Chunk** chunk_buffer, size_t size) {
      *chunk_buffer = random_chunk(size);
      char* data_buffer = new char[size];
      std::memcpy(data_buffer, (*chunk_buffer)->data, size);
      s->set(data_buffer, size);
    };

    auto verify_chunk = [](Session* s, Chunk* c, std::mutex& lock, unsigned int& ctr) {
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
      bool incorrect = false;
      for (int k = 0; k < size_buff; k++) {
        if (c->data[k] != data_buff[k]) {
          spdlog::error("{} INCORRECT VALUE: CHUNK={} BYTE={} EXPECTED_VALUE={} ACTUAL_VALUE={}", hex_string(s->self_key()), 
                        hex_string(c->key), k, static_cast<uint8_t>(c->data[k]), static_cast<uint8_t>(data_buff[k]));
          incorrect = true;
          return;
        }
      }
      if (incorrect) {
        return;
      }
      spdlog::debug("{} CHUNK CORRECT: CHUNK={}", hex_string(s->self_key()), hex_string(c->key));
      lock.lock();
      ctr++;
      lock.unlock();
    };

    // create servers
    std::vector<std::thread*> threads;
    for (int i = 0; i < num_servers; i++) {
      threads.push_back(new std::thread(create_session, &servers[i], i, num_servers));
    }
    while (threads.size() > 0) {
      threads.back()->join();
      delete threads.back();
      threads.pop_back();
    }

    // add server chunks
    for (int i = 0; i < num_servers; i++) {
      Session* s = servers[i];
      for (int j = 0; j < num_chunks; j++) {
        unsigned int chunk_idx = i * num_chunks + j;
        create_chunk(servers[i], &chunks[chunk_idx], chunk_idx);
      }
    }

    for (int i = 0; i < num_clients; i += 10) {

      // create client sessions
      for (int j = 0; j < 10; j++) {
        threads.push_back(new std::thread(create_session, &clients[i], i, 10));
      }
      while (threads.size() > 0) {
        threads.back()->join();
        delete threads.back();
        threads.pop_back();
      }
      
      // get the server and previous client chunks and verify
      for (int j = 0; j < num_chunks * (num_servers + 10 * i); j++) {
        for (int k = 0; k < 10; k++) {
          threads.push_back(new std::thread(verify_chunk, clients[k], chunks[j], correct_lock, num_correct));
        }
      }
      while (threads.size() > 0) {
        threads.back()->join();
        delete threads.back();
        threads.pop_back();
      }

      // add new client chunks
      for (int j = 0; j < 10; j++) {
        unsigned int chunk_idx = num_chunks * (num_servers + 10 * i) + j;
        threads.push_back(new std::thread(create_chunk, clients[j], &chunks[chunk_idx], chunk_idx));
      }
      while (threads.size() > 0) {
        threads.back()->join();
        delete threads.back();
        threads.pop_back();
      }

      // delete client sessions
      for (int j = 0; j < 10; j++) {
        threads.push_back(new std::thread([](Session* s){
          s->teardown(true);
          delete s;
        }, clients[j]));
      }
    }
    
    // delete chunks and server sessions
    for (int i = 0; i < num_chunks; i++) {
      delete chunks[i];
    }
    for (int i = 0; i < num_servers; i++) {
      threads.push_back(new std::thread([](Session* s) {
        s->teardown(false);
        delete s;
      }, servers[i]));
    }
    while (threads.size() > 0) {
      threads.back()->join();
      delete threads.back();
      threads.pop_back();
    }

    unsigned int num_client_groups = num_clients / 10;
    unsigned int num_expected = (num_servers + (num_client_groups * (num_client_groups - 1)) / 2) * 10;
    return num_correct == num_expected;
  };
  return fn;
}

// HELPERS

void create_session(unsigned int num_endpoints, unsigned int create_wait, unsigned int i) {
  unsigned int my_port = i;
  unsigned int other_port = (i + 1) % num_endpoints;
  char my_addr[20];
  char other_addr[20];
  sprintf(my_addr, "localhost:%i", 2000 + my_port);
  sprintf(other_addr, "localhost:%i", 2000 + other_port);
  Session* s = new Session;
  s->startup(std::string(my_addr), std::string(other_addr));
  std::this_thread::sleep_for(std::chrono::seconds(create_wait));
  s->teardown(false);
}

// create and destroy sessions (makes sure that all are successfully created without timing out)
bool create_destroy_sessions(unsigned int num_endpoints) {
  std::vector<std::thread*> threads;
  for (int i = 1; i < num_endpoints; i++) {
    std::thread* session_thread = new std::thread(create_session, num_endpoints, 3, i);
    threads.push_back(session_thread);
  }
  create_session(num_endpoints, 3, 0);
  while (threads.size() > 0) {
    std::thread* session_thread = threads.back();
    threads.pop_back();
    session_thread->join();
    delete session_thread;
  }
  return true;
}

Chunk* random_chunk(size_t size) {
  char* data_buffer = new char[size];
  for (int i = 0; i < size; i++) {
    data_buffer[i] = static_cast<char>(std::rand() % 0xFF);
  }
  return new Chunk(data_buffer, size, true, std::chrono::steady_clock::now());
}