#include "tests.h"

// HELPER DEFS.
Chunk* random_chunk(size_t size);
void create_session(Session*& session, unsigned int my_idx, unsigned int other_idx);
void create_chunk(Session* session, Chunk*& chunk, size_t size);
void verify_chunk(Session* s, Chunk* c, std::mutex& lock, unsigned int& ctr);
void wait_on_threads(std::vector<std::thread*>& threads);

//
// TEST FUNCTION GENERATORS
//

// create and destroy sessions (makes sure that all are successfully created without timing out)
std::function<bool()> create_destroy_sessions(unsigned int num_endpoints) {
  auto fn = [num_endpoints]() {
    Session* sessions[num_endpoints];
    std::vector<std::thread*> threads;
    for (int i = 0; i < num_endpoints; i++) {
      std::thread* session_thread = new std::thread(create_session, std::ref(sessions[i]), i, (i + 1) % num_endpoints);
      threads.push_back(session_thread);
    }
    wait_on_threads(threads);
    return true;
  };
  return fn;
}

// returns a function that creates/destroys sessions and stores/gets chunks
std::function<bool()> store_chunks_fn(unsigned int num_chunks, unsigned int num_endpoints) {
  auto fn = [num_chunks, num_endpoints]() {
    // track sessions/chunk storage
    Session* sessions[num_endpoints];
    Chunk* chunks[num_chunks];
    std::mutex correct_lock;
    unsigned int num_correct = 0;
    std::vector<std::thread*> threads;

    // create sessions
    for (int i = 0; i < num_endpoints; i++) {
      threads.push_back(new std::thread(create_session, std::ref(sessions[i]), i, (i + 1) % num_endpoints));
    }
    wait_on_threads(threads);

    // set chunks in session
    for (int i = 0; i < num_chunks; i++) {
      threads.push_back(new std::thread(create_chunk, sessions[0], std::ref(chunks[i]), i + 5));
    }
    wait_on_threads(threads);

    // create threads to verify
    for (int i = 0; i < num_endpoints; i++) {
      for (int j = 0; j < num_chunks; j++) {
        threads.push_back(new std::thread(verify_chunk, sessions[i], chunks[j], std::ref(correct_lock), std::ref(num_correct)));
      }
    }
    wait_on_threads(threads);

    // delete chunks and sessions
    // destroy all chunks and servers
    for (int i = 0; i < num_chunks; i++) {
      threads.push_back(new std::thread([](Chunk* c){
        delete c;
      }, chunks[i]));
    }
    for (int i = 0; i < num_endpoints; i++) {
      threads.push_back(new std::thread([](Session* s) {
        s->teardown(false);
        delete s;
      }, sessions[i]));
    }
    wait_on_threads(threads);
    return num_correct >= num_endpoints * num_chunks;
  };
  return fn;
}

std::function<bool()> churn_chunks_fn(unsigned int num_chunks, unsigned int num_servers, unsigned int num_clients, unsigned int chunk_tol) {
  unsigned int max_clients_per_burst = 10;
  auto fn = [num_chunks, num_servers, num_clients, chunk_tol, max_clients_per_burst]() {
    // track sessions/chunk storage
    Session* servers[num_servers];
    Chunk* chunks[(num_servers + num_clients) * num_chunks];
    unsigned int chunk_idx = 0;
    std::mutex correct_lock;
    unsigned int num_correct = 0;

    // create servers
    std::vector<std::thread*> threads;
    for (int i = 0; i < num_servers; i++) {
      threads.push_back(new std::thread(create_session, std::ref(servers[i]), i, (i + 1) % num_servers));
    }
    wait_on_threads(threads);

    unsigned int remaining_clients = num_clients;
    while (remaining_clients > 0) {
      // create client threads
      unsigned int client_burst = (std::rand() % std::min(max_clients_per_burst, remaining_clients)) + 1;
      Session* clients[client_burst];
      for (int i = 0; i < client_burst; i++) {
        threads.push_back(new std::thread(create_session, std::ref(clients[i]), num_servers + i, std::rand() % num_servers));
      }
      wait_on_threads(threads);

      // add client chunks
      for (int i = 0; i < client_burst; i++) {
        for (int j = 0; j < num_chunks; j++) {
          unsigned int my_chunk = chunk_idx++;
          threads.push_back(new std::thread(create_chunk, clients[i], std::ref(chunks[my_chunk]), std::rand() % 0xFFFF));
        }
      }
      wait_on_threads(threads);

      // destroy client threads (randomly drop or republish chunks)
      for (int i = 0; i < client_burst; i++) {
        threads.push_back(new std::thread([](Session* s){
          s->teardown(false);
          delete s;
        }, clients[i]));
      }
      wait_on_threads(threads);

      // add some dummy chunks to remove dead peers
      Chunk* temp_chunks[num_servers];
      for (int i = 0; i < num_servers; i++) {
        threads.push_back(new std::thread(create_chunk, servers[i], std::ref(temp_chunks[i]), 0));
      }
      wait_on_threads(threads);

      remaining_clients -= client_burst;
    }

    // add server chunks
    for (int i = 0; i < num_servers; i++) {
      Session* s = servers[i];
      for (int j = 0; j < num_chunks; j++) {
        unsigned int my_chunk = chunk_idx++;
        threads.push_back(new std::thread(create_chunk, servers[i], std::ref(chunks[my_chunk]), std::rand() % 0xFFFF));
      }
    }
    wait_on_threads(threads);

    // servers verify all chunks
    for (int i = 0; i < num_servers; i++) {
      for (int j = 0; j < num_chunks * (num_servers + num_clients); j++) {
        threads.push_back(new std::thread(verify_chunk, servers[i], chunks[j], std::ref(correct_lock), std::ref(num_correct)));
      }
    }
    wait_on_threads(threads);

    // destroy all chunks and servers
    for (int i = 0; i < (num_servers + num_clients); i++) {
      threads.push_back(new std::thread([](Chunk* c){
        delete c;
      }, chunks[i]));
    }
    for (int i = 0; i < num_servers; i++) {
      threads.push_back(new std::thread([](Session* s){
        s->teardown(false);
        delete s;
      }, servers[i]));
    }

    return num_correct >= (num_chunks * num_servers * (num_servers + num_clients)) - chunk_tol;
  };
  return fn;
}

//
// HELPERS
//

Chunk* random_chunk(size_t size) {
  Key key = random_key();
  std::vector<char>* data = new std::vector<char>;
  for (int i = 0; i < size; i++) {
    data->push_back(static_cast<char>(std::rand() % 0xFF));
  }
  return new Chunk(key, data, true, std::chrono::system_clock::now());
}


void create_session(Session*& session, unsigned int my_idx, unsigned int other_idx) {
  unsigned int my_port = my_idx;
  unsigned int other_port = other_idx;
  char my_addr[20];
  char other_addr[20];
  sprintf(my_addr, "localhost:%i", 2000 + my_port);
  sprintf(other_addr, "localhost:%i", 2000 + other_port);
  session = new Session;
  session->startup(std::string(my_addr), std::string(other_addr));
};


void create_chunk(Session* session, Chunk*& chunk, size_t size) {
  chunk = random_chunk(size);
  std::vector<char>* data = new std::vector<char>(chunk->data->begin(), chunk->data->end());
  session->set(chunk->key, data);
};

void wait_on_threads(std::vector<std::thread*>& threads) {
  while (threads.size() > 0) {
    threads.back()->join();
    delete threads.back();
    threads.pop_back();
  }
}

void verify_chunk(Session* s, Chunk* c, std::mutex& lock, unsigned int& ctr) {
  std::vector<char>* data_buff;
  bool found = s->get(c->key, &data_buff);
  if (!found) {
    spdlog::error("{} CHUNK NOT FOUND: CHUNK={}", hex_string(s->self_key()), hex_string(c->key));
    return;
  }
  if (c->data->size() != data_buff->size()) {
    spdlog::error("{} INCORRECT SIZE: CHUNK={} EXPECTED_SIZE={} ACTUAL_SIZE={}", hex_string(s->self_key()),
                    hex_string(c->key), c->data->size(), data_buff->size());
    return;
  }
  bool incorrect = false;
  for (int k = 0; k < data_buff->size(); k++) {
    if (c->data->at(k) != data_buff->at(k)) {
      spdlog::error("{} INCORRECT VALUE: CHUNK={} BYTE={} EXPECTED_VALUE={} ACTUAL_VALUE={}", hex_string(s->self_key()), 
                    hex_string(c->key), k, static_cast<uint8_t>(c->data->at(k)), static_cast<uint8_t>(data_buff->at(k)));
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
