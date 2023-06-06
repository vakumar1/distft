#include <utils.h>

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

  Chunk(Key key, std::vector<char>* data, bool original_publisher, 
        std::chrono::time_point<std::chrono::system_clock> original_publish) {
    this->key = key;
    this->data = data;
    this->original_publisher = original_publisher;
    this->original_publish = original_publish;
    this->last_published = std::chrono::system_clock::now();
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
  std::vector<char>* data;
  std::chrono::time_point<std::chrono::system_clock> last_published;
  std::chrono::time_point<std::chrono::system_clock> original_publish;
};
