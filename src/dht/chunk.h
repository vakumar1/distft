#include "key.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <openssl/sha.h>
#include <chrono>

#define CHUNK_DIR "~/.distft"
#define FILEPATH_SIZE 20

class Chunk {
private:
  std::string get_filepath() {
    std::stringstream ss;
    ss << std::hex << this->key.to_ulong();
    std::string file = ss.str();
    char buffer[FILEPATH_SIZE];
    std::sprintf(buffer, "%s/%s", CHUNK_DIR, file.c_str());
    return std::string(buffer);
  }

public:
  // TODO: store data in filesystem

  Chunk(std::byte* data, size_t size, bool original_publisher, std::chrono::time_point<std::chrono::steady_clock> expiry_time) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data), size, hash);
    this->key = Key();
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
      unsigned char byte = hash[i];
      for (int j = 0; j < 8; j++) {
        if (byte >> j & 0x1) {
          this->key.set(8 * i + j);
        } else {
          this->key.reset(8 * i + j);
        }
      }
    }
    this->size = size;
    this->data = data;
    this->original_publisher = original_publisher;
    this->last_published = std::chrono::steady_clock::now();
    this->expiry_time = expiry_time;
  }

  ~Chunk() {
    if (this->data != NULL) {
      delete this->data;
    }
  }

  // bool read_in_bytes() {
  //   if (this->data != NULL) {
  //     return true;
  //   }
  //   std::ifstream file(this->get_filepath(), std::ios::binary);
  //   if (!file) {
  //     return false;
  //   }
  //   this->data = new std::byte[CHUNK_BYTES];
  //   file.read(reinterpret_cast<char*>(this->data), CHUNK_BYTES);
  //   if (!file) {
  //     return false;
  //   }
  //   file.close();
  //   return true;
  // }

  // bool write_bytes() {
  //   std::ofstream file(this->get_filepath(), std::ios::binary);
  //   if (!file) {
  //     return false;
  //   }
  //   file.write(reinterpret_cast<char*>(this->data), CHUNK_BYTES);
  //   if (!file) {
  //     return false;
  //   }
  //   file.close();
  //   return true;
  // }
  bool original_publisher;
  Key key;
  std::byte* data;
  size_t size;
  std::chrono::time_point<std::chrono::steady_clock> last_published;
  std::chrono::time_point<std::chrono::steady_clock> expiry_time;
};
