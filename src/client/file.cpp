#include "file.h"

//
// INDEX FILE ACCESS/MUTATION
//

Key index_key = key_from_string(std::string("index"));

bool init_index_file(Session* s) {
  std::vector<char> dummy_data;
  s->set(index_key, &dummy_data);
  return true;
}

bool add_files_to_index_file(Session* s, std::vector<std::string> files) {
  std::vector<char>* data_buffer;
  if (!s->get(index_key, &data_buffer)) {
    return false;
  }
  for (std::string file : files) {
    data_buffer->insert(data_buffer->end(), file.begin(), file.end());
    data_buffer->push_back('\0');
  }
  s->set(index_key, data_buffer);
  return true;
}

bool get_index_files(Session* s, std::vector<std::string>& files_buffer) {
  std::vector<char>* data_buffer;
  if (!s->get(index_key, &data_buffer)) {
    return false;
  }

  std::string curr_file;
  for (int i = 0; i < data_buffer->size(); i++) {
    const char c = data_buffer->at(i);
    if (c == '\0') {
      if (curr_file.length() > 0) {
        files_buffer.push_back(curr_file);
      }
      curr_file.clear();
    }
    curr_file.push_back(c);
  }
  return true;
}

//
// GENERAL FILE ACCESS/MUTATION
//

const unsigned int max_chunk_size = 1048576;

bool write_from_file(Session* s, std::string file) {
  std::ifstream file_stream(file, std::ios::binary);
  if (!file_stream) {
      return false;
  }

  // write all data in file to the DHT
  std::vector<Key> chunks;
  while (true) {
    std::vector<char>* data = new std::vector<char>;
    file_stream.read(data->data(), max_chunk_size);
    if (data->size() == 0) {
      break;
    }
    Key key = key_from_data(data->data(), data->size());
    s->set(key, data);
    chunks.push_back(key);
  }

  // write all file keys to the metadata chunk
  Key metadata_key = key_from_string(file);
  std::vector<char>* metadata = new std::vector<char>;
  for (Key key : chunks) {
    metadata->insert(metadata->end(), key.to_string().begin(), key.to_string().end());
    metadata->push_back('\0');
  }
  s->set(metadata_key, metadata);
  return true;
}

bool read_in_file(Session* s, std::string file, std::vector<char>** file_data_buffer) {

}