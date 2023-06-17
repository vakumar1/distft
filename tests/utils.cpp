#include "tests.h"

#include <file.h>
#include <session.h>
#include <utils.h>

session_metadata dummy_meta;

Chunk* random_chunk(size_t size) {
  Key key = random_key();
  std::vector<char>* data = new std::vector<char>;
  for (int i = 0; i < size; i++) {
    data->push_back(static_cast<char>(std::rand() % 0xFF));
  }
  return new Chunk(key, data, true, std::chrono::system_clock::now());
}

void random_file(std::filesystem::path path, size_t size) {
  std::ofstream file(path, std::ios::binary);
  for (int i = 0; i < size; i++) {
    file << static_cast<char>(std::rand() % 0xFF);
  }
}

void create_session(Session*& session, unsigned int my_idx, unsigned int other_idx) {
  unsigned int my_port = my_idx;
  unsigned int other_port = other_idx;
  char my_addr[20];
  char other_addr[20];
  sprintf(my_addr, "localhost:%i", 2000 + my_port);
  sprintf(other_addr, "localhost:%i", 2000 + other_port);
  session = new Session;
  session->startup(&dummy_meta, std::string(my_addr), std::string(other_addr));
};


void create_chunk(Session* session, Chunk*& chunk, size_t size) {
  chunk = random_chunk(size);
  std::vector<char>* data = new std::vector<char>(chunk->data->begin(), chunk->data->end());
  session->set(chunk->key, data, false);
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

void verify_file(Session* s, std::vector<char>* file_data, std::string dht_filename, 
                  std::mutex& lock, unsigned int& found_ctr, unsigned int& corr_ctr) {

  if (!file_exists(s, dht_filename)) {
    spdlog::error("{} FILE NOT FOUND: file={}", hex_string(s->self_key()), dht_filename);
    return;
  }
  
  lock.lock();
  found_ctr++;
  lock.unlock();

  std::vector<char>* buff = new std::vector<char>;
  if (!read_in_files(s, std::vector<std::string>{dht_filename}, &buff)) {
    spdlog::error("{} ERROR WHILE READING FILE IN: file={}", hex_string(s->self_key()), dht_filename);
    return;
  }

  if (file_data->size() != buff->size()) {
    spdlog::error("{} FILE HAS INCORRECT SIZE: file={} expected={} actual={}", 
                  hex_string(s->self_key()), dht_filename, file_data->size(), buff->size());
    return;
  }

  for (int j = 0; j < file_data->size(); j++) {
    if (file_data->at(j) != buff->at(j)) {
      spdlog::error("{} FILE HAS INCORRECT DATA: file={} byte={} expected={} actual={}", 
                  hex_string(s->self_key()), dht_filename, j, file_data->at(j), buff->at(j));
      return;
    }
  }
  lock.lock();
  corr_ctr++;
  lock.unlock();
  spdlog::info("{} FILE CORRECTLY STORED: file={}", hex_string(s->self_key()), dht_filename);
}