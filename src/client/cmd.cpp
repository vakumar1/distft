#include "client.h"

std::string help_cmd() {
std::string help = 
R"(  help: print all commands
  start <endpoint 1> ... <endpoint n>: create a new session cluster with at least 2 endpoints (prints out the session id)
  list <session id>: print all files
  store <session id> <file path 1> ... <file path n>: add file(s) at local file path(s) to session
  load <session id> <file name 1> ... <file name n> <file path>: write the content of file name(s) to local file path
  exit <session id>: exit the session
  ** only include a session id if in non-interactive mode and working with a running daemon
)";
  return help;
}

// bootstrap a new cluster of sessions from at least 2 endpoints
bool bootstrap_cmd(client_state& state, std::vector<std::string> endpoints) {
  state.dying = false;
  state.endpoints = endpoints;
  state.meta = new session_metadata;
  std::vector<std::thread*> threads;

  // startup sessions
  for (int i = 0; i < state.endpoints.size(); i++) {
    Session* s = new Session;
    state.sessions.push_back(s);
    threads.push_back(new std::thread([state](Session* s, std::string my_ep, std::string other_ep) {
      s->startup(state.meta, my_ep, other_ep);
    }, s, endpoints[i], endpoints[(i + 1) % endpoints.size()]));
  }
  while (threads.size() > 0) {
    threads.back()->join();
    delete threads.back();
    threads.pop_back();
  }

  // init index file
  if (!init_index_file(state.sessions[std::rand() % state.sessions.size()])) {
    exit_cmd(state);
    state.cmd_err += "Failed to initialize session index file\n";
    return false;
  }
  state.cmd_out += "Initialized sessions and index file for new cluster.\n";
  return true;
}

// add a session strain reliever background thread
bool start_background_strain_reliever_cmd(client_state& state) {
  state.meta->dead_peers_thresh = 15;
  const unsigned int max_dummy_chunks = 10;
  auto strain_checker = [](client_state& state) {
    while (true) {
      std::unique_lock<std::mutex> uq_meta_lock(state.meta->meta_lock);
      while (!state.dying && state.meta->dead_peers >= state.meta->dead_peers_thresh) {
        state.meta->dead_cv.wait(uq_meta_lock);
      }
      state.meta->dead_peers = 0;
      uq_meta_lock.unlock();
      if (state.dying) {
        return;
      }
      for (int i = 0; i < std::rand() % max_dummy_chunks; i++) {
        state.sessions[std::rand() % state.sessions.size()]->set(random_key(), new std::vector<char>());
      }
    }
  };

  // start background strain management thread
  state.background_threads.push_back(new std::thread(strain_checker, std::ref(state)));
  return true;
}

// join an existing session cluster
bool join_cmd(client_state& state, std::string my_endpoint, std::string ext_endpoint) {
  state.endpoints.push_back(my_endpoint);
  state.meta = new session_metadata;
  Session* s = new Session;
  state.sessions.push_back(s);
  s->startup(state.meta, my_endpoint, ext_endpoint);
  state.cmd_out += "Joined external session successfully.\n";
  return true;
}

// add a list of all files in the session to the state's cmd output
bool list_cmd(client_state& state) {
  std::vector<std::string> index_files;
  if (!get_index_files(state.sessions[std::rand() % state.sessions.size()], index_files)) {
    state.cmd_err += "Failed to access index file\n";
    return false;
  }
  state.cmd_out.append(std::string("INDEX\n"));
  for (std::string file : index_files) {
    state.cmd_out.append(file);
    state.cmd_out.push_back('\n');
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
        || file_exists(state.sessions[std::rand() % state.sessions.size()], dht_filename)) {
      state.cmd_err += "File" + file + " already exists in the current session. Skipping.\n";
      continue;
    }
    if (!write_from_file(state.sessions[std::rand() % state.sessions.size()], file, dht_filename)) {
      state.cmd_err += "File " +  file + " does not exist or read failed. Skipping.\n";
      continue;
    }
    added_files.push_back(dht_filename);
  }
  if (!add_files_to_index_file(state.sessions[std::rand() % state.sessions.size()], added_files)) {
    state.cmd_err += "Failed to write to index file.\n";
    return false;
  }
  return true;
}

// load (and concatenate) all files to a local output file
bool load_cmd(client_state& state, std::vector<std::string> input_files, std::string output_file) {
  std::vector<char>* buffer;
  if (!read_in_files(state.sessions[std::rand() % state.sessions.size()], input_files, &buffer)) {
    state.cmd_err += "Failed to read from files\n";
    return false;
  }
  std::ofstream file(output_file, std::ios::out | std::ios::binary);
  if (!file) {
    state.cmd_err += "Failed to open output file " + output_file;
    return false;
  }
  file.write(buffer->data(), buffer->size());
  file.close();
  return true;
}

// destroy the current session
void exit_cmd(client_state& state) {
  state.dying = true;
  state.meta->dead_cv.notify_all();
  while (state.background_threads.size() > 0) {
    state.background_threads.back()->join();
    delete state.background_threads.back();
    state.background_threads.pop_back();
  }
  std::vector<std::thread*> threads;
  for (int i = 0; i < state.sessions.size(); i++) {
    threads.push_back(new std::thread([](Session* s) {
      s->teardown(false);
      delete s;
    }, state.sessions[i]));
  }
  while (threads.size() > 0) {
    threads.back()->join();
    delete threads.back();
    threads.pop_back();
  }
  delete state.meta;
}
