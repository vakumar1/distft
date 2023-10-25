#include "file.h"

#include "src/utils/utils.h"

//
// INDEX FILE ACCESS/MUTATION
//

// index file key
Key index_key = key_from_string(std::string("index"));

// initialize the index file in the session
bool init_index_file(Session* s) {
  std::vector<char>* dummy_data = new std::vector<char>;
  s->set(index_key, dummy_data, true);
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
  s->set(index_key, data_buffer, true);
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

  // (concurrently) write all data in file to the DHT
  std::vector<Key> chunks;
  std::vector<char> buffer(max_chunk_size);
  std::vector<std::thread> threads;
  while (!file_stream.eof()) {
    file_stream.read(buffer.data(), max_chunk_size);
    std::size_t bytes_read = static_cast<std::size_t>(file_stream.gcount());
    std::vector<char>* data = new std::vector<char>(buffer.begin(), buffer.begin() + bytes_read);
    Key key = key_from_data(data->data(), data->size());
    threads.push_back(std::thread(
      [s](Key key, std::vector<char>* data) { 
        s->set(key, data, false); 
      }, key, data
    ));
    chunks.push_back(key);
  }
  while (threads.size() > 0) {
    threads.back().join();
    threads.pop_back();
  }

  // write all file keys to the metadata chunk
  Key metadata_key = key_from_string(dht_filename);
  std::vector<char>* metadata = new std::vector<char>;
  for (Key key : chunks) {
    std::string key_str = key.to_string();
    metadata->insert(metadata->end(), key_str.begin(), key_str.end());
    metadata->push_back('\0');
  }
  s->set(metadata_key, metadata, true);
  return true;
}

// read the file from session to local buffer
bool read_in_files(Session* s, std::vector<std::string> files, std::vector<char>** file_data_buffer) {
  std::vector<Key> ordered_keys;

  // read in the keys for all files
  for (std::string file : files) {
    Key metadata_key = key_from_string(file);
    std::vector<char>* metadata_chunk;
    if (!s->get(metadata_key, &metadata_chunk)) {
      return false;
    }

    // read in the file keys in the metadata chunk
    std::string curr_key;
    for (int i = 0; i < metadata_chunk->size(); i++) {
      const char c = metadata_chunk->at(i);
      if (c == '\0') {
        if (curr_key.length() != KEYBITS) {
          spdlog::error("{} MALFORMED METADATA FILE (INCORRECTLY SIZED CHUNK KEY): CHUNK={}", hex_string(metadata_key), curr_key);
        } else {
          ordered_keys.push_back(Key(curr_key));
        }
        curr_key.clear();
        continue;
      }
      curr_key.push_back(c);
    }
  }


  // (concurrently) read in all file chunks and store in buffer
  std::vector<std::vector<char>*> ordered_chunks(ordered_keys.size(), NULL);
  std::vector<std::thread> threads;
  for (unsigned int i = 0; i < ordered_keys.size(); i++) {
    threads.push_back(std::thread(
      [s](Key key, std::vector<char>** data) {
        s->get(key, data);
      }, ordered_keys[i], &ordered_chunks[i]
    ));
  }
  while (threads.size() > 0) {
    threads.back().join();
    threads.pop_back();
  }
  

  bool success = true;
  *file_data_buffer = new std::vector<char>;
  for (std::vector<char>* chunk : ordered_chunks) {
    if (chunk == NULL) {
      success = false;
    } else {
      if (success) {
        (*file_data_buffer)->insert((*file_data_buffer)->end(), chunk->begin(), chunk->end());
      }
      delete chunk;
    }
  }
  if (!success) {
    delete (*file_data_buffer);
  }
  return success;
}
