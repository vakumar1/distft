#include "client.h"
#include "file.h"

#include <session.h>
#include <utils.h>

#include <filesystem>

// client state: defines the internal data for the current running client
struct client_state::client_state_data {
  bool dying;
  std::vector<std::string> endpoints;
  std::vector<Session*> sessions;
  std::vector<std::thread*> background_threads;
  std::string cmd_err;
  std::string cmd_out;
  session_metadata* meta;
};
client_state::client_state() {
  this->data = new client_state::client_state_data;
}
client_state::~client_state() {
  delete this->data;
}
std::string client_state::get_cmd_err() {
  return this->data->cmd_err;
}
std::string client_state::get_cmd_out() {
  return this->data->cmd_out;
}
void client_state::clear_cmd() {
  this->data->cmd_err.clear();
  this->data->cmd_out.clear();
}

// bootstrap a new cluster of sessions from at least 2 endpoints
bool bootstrap_cmd(client_state& state, std::vector<std::string> endpoints) {
  state.data->dying = false;
  state.data->endpoints = endpoints;
  state.data->meta = new session_metadata;
  std::vector<std::thread*> threads;

  // startup sessions
  for (int i = 0; i < state.data->endpoints.size(); i++) {
    Session* s = new Session;
    state.data->sessions.push_back(s);
    threads.push_back(new std::thread([state](Session* s, std::string my_ep, std::string other_ep) {
      s->startup(state.data->meta, my_ep, other_ep);
    }, s, endpoints[i], endpoints[(i + 1) % endpoints.size()]));
  }
  while (threads.size() > 0) {
    threads.back()->join();
    delete threads.back();
    threads.pop_back();
  }

  // init index file
  if (!init_index_file(state.data->sessions[std::rand() % state.data->sessions.size()])) {
    exit_cmd(state);
    state.data->cmd_err = "Failed to initialize session index file";
    return false;
  }
  state.data->cmd_out = "Initialized sessions and index file for new cluster.";
  return true;
}

// add a session strain reliever background thread
bool start_background_strain_reliever_cmd(client_state& state) {
  state.data->meta->dead_peers_thresh = 15;
  const unsigned int max_dummy_chunks = 10;
  auto strain_checker = [](client_state& state) {
    while (true) {
      std::unique_lock<std::mutex> uq_meta_lock(state.data->meta->meta_lock);
      while (!state.data->dying && state.data->meta->dead_peers >= state.data->meta->dead_peers_thresh) {
        state.data->meta->dead_cv.wait(uq_meta_lock);
      }
      state.data->meta->dead_peers = 0;
      uq_meta_lock.unlock();
      if (state.data->dying) {
        return;
      }
      for (int i = 0; i < std::rand() % max_dummy_chunks; i++) {
        state.data->sessions[std::rand() % state.data->sessions.size()]->set(random_key(), new std::vector<char>(), false);
      }
    }
  };

  // start background strain management thread
  state.data->background_threads.push_back(new std::thread(strain_checker, std::ref(state)));
  return true;
}

// join an existing session cluster
bool join_cmd(client_state& state, std::string my_endpoint, std::string ext_endpoint) {
  state.data->endpoints.push_back(my_endpoint);
  state.data->meta = new session_metadata;
  Session* s = new Session;
  state.data->sessions.push_back(s);
  s->startup(state.data->meta, my_endpoint, ext_endpoint);
  state.data->cmd_out = "Joined external session successfully.";
  return true;
}

// add a list of all files in the session to the state's cmd output
bool list_cmd(client_state& state) {
  std::vector<std::string> index_files;
  if (!get_index_files(state.data->sessions[std::rand() % state.data->sessions.size()], index_files)) {
    state.data->cmd_err = "Failed to access index file";
    return false;
  }
  state.data->cmd_out.append(std::string("INDEX\n"));
  for (std::string file : index_files) {
    state.data->cmd_out.append(file);
    state.data->cmd_out.push_back('\n');
  }
  return true;
}

// store all files in the session
bool store_cmd(client_state& state, std::vector<std::string> files) {
  std::vector<std::string> added_files;
  for (std::string file : files) {
    std::filesystem::path file_path(file);
    std::string dht_filename = file_path.filename().string();
    if (std::find(added_files.begin(), added_files.end(), dht_filename) != added_files.end()
        || file_exists(state.data->sessions[std::rand() % state.data->sessions.size()], dht_filename)) {
      state.data->cmd_err += "File" + file + " already exists in the current session. Skipping.";
      continue;
    }
    if (!write_from_file(state.data->sessions[std::rand() % state.data->sessions.size()], file, dht_filename)) {
      state.data->cmd_err += "File " +  file + " does not exist or read failed. Skipping.";
      continue;
    }
    added_files.push_back(dht_filename);
  }
  if (!add_files_to_index_file(state.data->sessions[std::rand() % state.data->sessions.size()], added_files)) {
    state.data->cmd_err += "Failed to write to index file.";
    return false;
  }
  state.data->cmd_out = "Successfully stored the following files\n";
  for (std::string file : added_files) {
    state.data->cmd_out += file;
    state.data->cmd_out.push_back('\n');
  }
  return true;
}

// load (and concatenate) all files to a local output file
bool load_cmd(client_state& state, std::vector<std::string> input_files, std::string output_file) {
  std::vector<char>* buffer;
  if (!read_in_files(state.data->sessions[std::rand() % state.data->sessions.size()], input_files, &buffer)) {
    state.data->cmd_err = "Failed to read from files";
    return false;
  }
  std::ofstream file(output_file, std::ios::out | std::ios::binary);
  if (!file) {
    state.data->cmd_err = "Failed to open output file " + output_file;
    return false;
  }
  file.write(buffer->data(), buffer->size());
  file.close();
  state.data->cmd_out = "Successfully loaded all files into output file " + output_file;
  return true;
}

// destroy the current session
void exit_cmd(client_state& state) {
  state.data->dying = true;
  state.data->meta->dead_cv.notify_all();
  while (state.data->background_threads.size() > 0) {
    state.data->background_threads.back()->join();
    delete state.data->background_threads.back();
    state.data->background_threads.pop_back();
  }
  std::vector<std::thread*> threads;
  for (int i = 0; i < state.data->sessions.size(); i++) {
    threads.push_back(new std::thread([](Session* s) {
      s->teardown(false);
      delete s;
    }, state.data->sessions[i]));
  }
  while (threads.size() > 0) {
    threads.back()->join();
    delete threads.back();
    threads.pop_back();
  }
  delete state.data->meta;
}
