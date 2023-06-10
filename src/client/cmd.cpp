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

bool start_cmd(std::vector<Session*>& sessions, std::vector<std::string> endpoints, std::string& msg) {
  std::vector<std::thread*> threads;
  for (int i = 0; i < endpoints.size(); i++) {
    Session* s = new Session;
    sessions.push_back(s);
    threads.push_back(new std::thread([](Session* s, std::string my_ep, std::string other_ep) {
      s->startup(my_ep, other_ep);
    }, s, endpoints[i], endpoints[(i + 1) % endpoints.size()]));
  }
  while (threads.size() > 0) {
    threads.back()->join();
    delete threads.back();
    threads.pop_back();
  }
  if (!init_index_file(sessions[std::rand() % sessions.size()])) {
    msg += "Failed to initialize session index file\n";
    return false;
  }
  return true;
}

bool list_cmd(std::vector<Session*> sessions, std::string& msg) {
  std::vector<std::string> index_files;
  if (!get_index_files(sessions[std::rand() % sessions.size()], index_files)) {
    msg += "Failed to access index file\n";
    return false;
  }
  for (std::string file : index_files) {
    msg.append(file);
    msg.push_back('\n');
  }
  return true;
}

bool store_cmd(std::vector<Session*> sessions, std::vector<std::string> files, std::string& msg) {
  std::vector<std::string> added_files;
  for (std::string file : files) {
    if (!write_from_file(sessions[std::rand() % sessions.size()], file)) {
      msg += "File " +  file + " does not exist or read failed. Skipping.\n";
      continue;
    }
    added_files.push_back(file);
  }
  if (!add_files_to_index_file(sessions[std::rand() % sessions.size()], added_files)) {
    msg += "Failed to write to index file.\n";
    return false;
  }
  return true;
}

bool load_cmd(std::vector<Session*> sessions, std::vector<std::string> input_files, std::string output_file, std::string& msg) {
  std::vector<char>* buffer;
  if (!read_in_files(sessions[std::rand() % sessions.size()], input_files, &buffer)) {
    msg += "Failed to read from files\n";
    return false;
  }
  std::ofstream file(output_file, std::ios::out | std::ios::binary);
  if (!file) {
    msg += "Failed to open output file " + output_file;
    return false;
  }
  file.write(buffer->data(), buffer->size());
  file.close();
  return true;
}

void exit_cmd(std::vector<Session*> sessions) {
  std::vector<std::thread*> threads;
  for (int i = 0; i < sessions.size(); i++) {
    threads.push_back(new std::thread([](Session* s) {
      s->teardown(false);
      delete s;
    }, sessions[i]));
  }
  while (threads.size() > 0) {
    threads.back()->join();
    delete threads.back();
    threads.pop_back();
  }
}
