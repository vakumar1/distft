#include "client.h"

//
// INDEX FILE ACCESS/MUTATION
//

// index file key
Key index_key = key_from_string(std::string("index"));

// initialize the index file in the session
bool init_index_file(Session* s) {
  std::vector<char>* dummy_data = new std::vector<char>;
  s->set(index_key, dummy_data);
  return true;
}

// add the files to the index file chunk
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

// list all files in the index file (i.e., read the contents of the index file)
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
    } else {
      curr_file.push_back(c);
    }
  }
  return true;
}

// return true if file already exists in session
bool file_exists(Session* s, std::string dht_filename) {
  std::vector<char>* data_buffer;
  if (!s->get(index_key, &data_buffer)) {
    return false;
  }

  std::string curr_file;
  for (int i = 0; i < data_buffer->size(); i++) {
    const char c = data_buffer->at(i);
    if (c == '\0') {
      if (curr_file == dht_filename) {
        return true;
      }
      curr_file.clear();
    } else {
      curr_file.push_back(c);
    }
  }
  return false;
}

//
// GENERAL FILE ACCESS/MUTATION
//

const unsigned int max_chunk_size = 1048576;

// write the file from local file system to session
bool write_from_file(Session* s, std::string file, std::string dht_filename) {
  std::ifstream file_stream(file, std::ios::binary);
  if (!file_stream) {
      return false;
  }

  // write all data in file to the DHT
  std::vector<Key> chunks;
  std::vector<char> buffer(max_chunk_size);
  while (!file_stream.eof()) {
    file_stream.read(buffer.data(), max_chunk_size);
    std::size_t bytes_read = static_cast<std::size_t>(file_stream.gcount());
    std::vector<char>* data = new std::vector<char>(buffer.begin(), buffer.begin() + bytes_read);
    Key key = key_from_data(data->data(), data->size());
    s->set(key, data);
    chunks.push_back(key);
  }

  // write all file keys to the metadata chunk
  Key metadata_key = key_from_string(dht_filename);
  std::vector<char>* metadata = new std::vector<char>;
  for (Key key : chunks) {
    std::string key_str = key.to_string();
    metadata->insert(metadata->end(), key_str.begin(), key_str.end());
    metadata->push_back('\0');
  }
  s->set(metadata_key, metadata);
  return true;
}

// read the file from session to local buffer
bool read_in_files(Session* s, std::vector<std::string> files, std::vector<char>** file_data_buffer) {
  std::vector<char>* file_data = new std::vector<char>;
  for (std::string file : files) {
    Key metadata_key = key_from_string(file);
    std::vector<char>* metadata_chunk;
    if (!s->get(metadata_key, &metadata_chunk)) {
      return false;
    }

    // read in the file keys in the metadata chunk
    std::vector<Key> file_chunks;
    std::string curr_key;
    for (int i = 0; i < metadata_chunk->size(); i++) {
      const char c = metadata_chunk->at(i);
      if (c == '\0') {
        if (curr_key.length() != KEYBITS) {
          spdlog::error("{} MALFORMED METADATA FILE (INCORRECTLY SIZED CHUNK KEY): CHUNK={}", hex_string(metadata_key), curr_key);
        } else {
          file_chunks.push_back(Key(curr_key));
        }
        curr_key.clear();
        continue;
      }
      curr_key.push_back(c);
    }

    // read in file chunks and add to end of buffer
    unsigned int pos = 0;
    for (Key chunk : file_chunks) {
      std::vector<char>* chunk_buffer;
      if (!s->get(chunk, &chunk_buffer)) {
        return false;
      }
      file_data->insert(file_data->end(), chunk_buffer->begin(), chunk_buffer->end());
      delete chunk_buffer;
    }
  }
  *file_data_buffer = file_data;
  return true;
}
