#include "client.h"

// INTERACTIVE (LOOP) CLIENT
int run_founder_session_interactive() {
  std::cout << "Enter >= 2 accessible endpoints for master server: ";
  std::string line;
  std::getline(std::cin, line);
  std::istringstream iss(line);
  std::vector<std::string> endpoints;
  std::string endpoint;
  while (iss >> endpoint) {
    endpoints.push_back(endpoint);
  }
  if (endpoints.size() < 2) {
    std::cout << "Enter >= 2 endpoints" << std::endl;
    return 0;
  }
  
  client_state state;
  if (!bootstrap_cmd(state, endpoints) || !start_background_strain_reliever_cmd(state)) {
    std::cout << "Error occurred in daemon while processing command. :(" << std::endl;
    std::cout << state.cmd_err.c_str() << std::endl;
    return 0;
  }

  // client loop
  bool success;
  while (true) {
    state.cmd_err.clear();
    state.cmd_out.clear();
    std::cout << "> ";
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
      std::cout << interactive_commands().c_str() << std::endl;
    } else if (tokens[0] == "list") {
      success = list_cmd(state);
    } else if (tokens[0] == "store") {
      if (tokens.size() < 2) {
        std::cout << "Please provide file(s) to add." << std::endl;
        continue;
      }
      std::vector<std::string> files;
      for (int i = 1; i < tokens.size(); i++) {
        files.push_back(tokens[i]);
      }
      success = store_cmd(state, files);
    } else if (tokens[0] == "load") {
      if (tokens.size() < 3) {
        std::cout << "Please provide file to print/file to write to." << std::endl;
        continue;
      }
      std::vector<std::string> files;
      for (int i = 1; i < tokens.size() - 1; i++) {
        files.push_back(tokens[i]);
      }
      std::string output_file = tokens.back();
      success = load_cmd(state, files, output_file);
    } else if (tokens[0] == "exit") {
      exit_cmd(state);
      exit(0);
    } else {
      std::cout << "Enter a valid command. Enter `help` for all commands." << std::endl;
      continue;
    }
    if (!success) {
      std::cout << "Error occurred in daemon while processing command. :(" << std::endl;
    }
    if (!state.cmd_err.empty()) {
      std::cout << state.cmd_err.c_str() << std::endl;
    }
    if (!state.cmd_out.empty()) {
      std::cout << state.cmd_out.c_str() << std::endl;
    }
  }
  return 0;
}

std::string interactive_commands() {
  return R"(
  help: print all commands
  list: print all files
  store <file path 1> ... <file path n>: add file(s) at local file path(s) to session
  load <file name 1> ... <file name n> <file path>: write the content of file name(s) to local file path
  exit: exit the session
)";
}