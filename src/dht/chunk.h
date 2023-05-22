#include "key.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#define CHUNK_BYTES 1024

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

  // Chunk(Key key) {
  //   this->key = key;
  //   this->data = NULL;
  // };
  Chunk(std::byte* data) {
    this->key = random_key();
    this->data = data;
  }
  Chunk(Key key, std::byte* data) {
    this->key = key;
    this->data = data;
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
  Key key;
  std::byte* data;
};
