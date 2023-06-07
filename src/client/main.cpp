#include "file.h"

#include <session.h>
#include <utils.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <thread>
#include <random>

void help_cmd() {
std::string help = R"(
help: print all commands
list: print all files
add <file path 1> ... <file path 2>: add file(s) at file path(s) to session
print <file name>: print out contents of file name
load <file name> <file path>: write the content of file name to file path
exit: exit the session
)";
  printf(help.c_str());
}

void list_cmd(std::vector<Session*> sessions) {
  std::vector<std::string> index_files;
  get_index_files(sessions[std::rand() % sessions.size()], index_files);
  for (std::string file : index_files) {
    std::cout << file << std::endl;
  }
}

void add_cmd(std::vector<Session*> sessions, std::vector<std::string> tokens) {
  std::vector<std::string> files;
  for (int i = 1; i < tokens.size(); i++) {
    std::string file = tokens[i];
    if (!write_from_file(sessions[std::rand() % sessions.size()], file)) {
      printf("File %s does not exist or read failed. Skipping.\n", file.c_str());
      continue;
    }
    files.push_back(file);
  }
  if (!add_files_to_index_file(sessions[std::rand() % sessions.size()], files)) {
    printf("Failed to write to index file.\n");
  }
}

void print_file_cmd(std::vector<Session*> sessions, std::vector<std::string> tokens) {
  std::string file = tokens[1];
  std::vector<char>* buffer;
  if (!read_in_file(sessions[std::rand() % sessions.size()], file, &buffer)) {
    printf("Failed to read from file %s\n", file.c_str());
    return;
  }
  for (int i = 0; i < buffer->size(); i++) {
    std::cout << buffer->at(i);
  }
  std::cout << std::endl;
}

void load_file_cmd(std::vector<Session*> sessions, std::vector<std::string> tokens) {
  std::string file = tokens[1];
  std::vector<char>* buffer;
  if (!read_in_file(sessions[std::rand() % sessions.size()], file, &buffer)) {
    printf("Failed to read from file %s\n", file.c_str());
    return;
  }
  std::ofstream output_file(tokens[2], std::ios::out | std::ios::binary);
  if (!output_file) {
    printf("Failed to open output file %s\n", tokens[2].c_str());
    return;
  }
  output_file.write(buffer->data(), buffer->size());
  output_file.close();
}

void exit_cmd(std::vector<Session*> sessions) {
  printf("Deleting master server...\n");
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
  printf("Exiting!\n");
}

int main(void) {
  // get session endpoints from user
  printf("Enter (>= 2) accessible endpoints for master server: ");
  std::string line;
  std::getline(std::cin, line);
  std::istringstream iss(line);
  std::vector<std::string> endpoints;
  std::string endpoint;
  while (iss >> endpoint) {
    endpoints.push_back(endpoint);
  }
  if (endpoints.size() < 2) {
    printf("Enter >= 2 endpoints\n");
    return 0;
  }

  // startup sessions
  printf("Starting master server...\n");
  std::vector<Session*> sessions;
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
  init_index_file(sessions[std::rand() % sessions.size()]);

  // start client loop
  printf("Master server started. Enter a command. Enter `help` for all commands.\n");
  while (true) {
    printf("\n> ");
    std::string line;
    std::getline(std::cin, line);
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
      tokens.push_back(token);
    }

    if (tokens.size() == 0) {
      continue;
    } else if (tokens[0] == "help") {
      help_cmd();
    } else if (tokens[0] == "list") {
      list_cmd(sessions);
    } else if (tokens[0] == "add") {
      if (tokens.size() < 2) {
        printf("Please provide file(s) to add.\n");
        continue;
      }
      add_cmd(sessions, tokens);
    } else if (tokens[0] == "print") {
      if (tokens.size() < 2) {
        printf("Please provide file to print.\n");
        continue;
      }
      print_file_cmd(sessions, tokens);
    } else if (tokens[0] == "load") {
      if (tokens.size() < 3) {
        printf("Please provide file to print/file to write to.\n");
        continue;
      }
      load_file_cmd(sessions, tokens);
    } else if (tokens[0] == "exit") {
      exit_cmd(sessions);
      return 0;
    } else {
      printf("Enter a valid command. Enter `help` for all commands.\n");
    }
  }
}
