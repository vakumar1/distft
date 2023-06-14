#include "client.h"

int interactive_loop() {
// INTERACTIVE (LOOP) CLIENT
  printf("Enter >= 2 accessible endpoints for master server: ");
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
  
  client_state state;
  if (!bootstrap_cmd(state, endpoints) || !start_background_strain_reliever_cmd(state)) {
    printf("Error occurred in daemon while processing command. :(\n");
    printf(state.cmd_err.c_str());
    return 0;
  }

  // client loop
  bool success;
  std::string output;
  while (true) {
    state.cmd_err.clear();
    state.cmd_out.clear();
    output.clear();
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
      printf(help_cmd().c_str());
    } else if (tokens[0] == "list") {
      success = list_cmd(state);
      output = success ? state.cmd_out : state.cmd_err;
    } else if (tokens[0] == "store") {
      if (tokens.size() < 2) {
        printf("\nPlease provide file(s) to add.");
        continue;
      }
      std::vector<std::string> files;
      for (int i = 1; i < tokens.size(); i++) {
        files.push_back(tokens[i]);
      }
      success = store_cmd(state, files);
      output = success ? state.cmd_err + state.cmd_out : state.cmd_err;
    } else if (tokens[0] == "load") {
      if (tokens.size() < 3) {
        printf("\nPlease provide file to print/file to write to.");
        continue;
      }
      std::vector<std::string> files;
      for (int i = 1; i < tokens.size() - 1; i++) {
        files.push_back(tokens[i]);
      }
      std::string output_file = tokens.back();
      success = load_cmd(state, files, output_file);
      output = success ? state.cmd_err + state.cmd_out : state.cmd_err;
    } else if (tokens[0] == "exit") {
      exit_cmd(state);
      exit(0);
    } else {
      printf("\nEnter a valid command. Enter `help` for all commands.");
      continue;
    }
    if (!success) {
      printf("Error occurred in daemon while processing command. :(\n");
    }
    printf(output.c_str());
  }
}

int main(int argc, char* argv[]) {
  // parse arguments for flags
  if (argc < 2) {
    printf("Provide arg(s). Enter `founder help` for all commands.\n");
    return 0;
  }
  bool interactive = false;
  bool help = false;
  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg == "-i" || arg == "--interactive") {
      interactive = true;
    }
    if (arg == "-h" || arg == "--help") {
      help = true;
    }
  }
  if (help) {
    printf(help_cmd().c_str());
    return 0;
  }
  if (interactive) {
    interactive_loop();
    return 0;
  }

  // DAEMON-BASED CLIENT
  // setup client and logger
  if (!setup_client()) {
    printf("Failed to setup client directory in ~.\n");
    return 1;
  }

  std::string cmd(argv[1]);
  char err;
  std::string msg;
  if (cmd == "start") {
    if (argc < 4) {
      printf("Provide at least 2 accessible endpoints for the founder client.\n");
      return 0;
    }
    std::vector<std::string> endpoints;
    for (int i = 2; i < argc; i++) {
      endpoints.push_back(std::string(argv[i]));
    }
    pid_t client_id = getpid();
    pid_t session_id = fork();
    if (session_id < 0) {
      printf("Failed to create new daemon. Aborting. :(\n");
      return 1;
    }
    if (session_id == 0) {
      setup_daemon();
      run_founder_session_daemon(client_id, endpoints);
      return 0;
    }
    add_daemon_files(session_id);
    if (!read_err(session_id, err, msg)) {
      printf("Failed to receive result from daemon. :(\n");
      return 1;
    }
    printf("SESSION ID: %d\n", session_id);
    if (static_cast<bool>(err)) {
      printf("Error occurred in daemon while processing command. :(\n");
    }
    printf(msg.c_str());
  } else if (cmd == "list") {
    if (argc < 3) {
      printf("Provide session id for a running founder client.\n");
      return 0;
    }
    int session_id = std::stoi(argv[2]);
    if (!write_cmd(session_id, LIST, 0, std::vector<std::string>())) {
      printf("Failed to send list cmd to daemon. :(\n");
      return 1;
    }
    if (!read_err(session_id, err, msg)) {
      printf("Failed to receive result from daemon. :(\n");
      return 1;
    }
    if (static_cast<bool>(err)) {
      printf("Error occurred in daemon while processing command. :(\n");
    }
    printf(msg.c_str());
  } else if (cmd == "store") {
    if (argc < 4) {
      printf("Provide session id for a running founder client and at least one file.\n");
      return 0;
    }
    int session_id = std::stoi(argv[2]);
    char file_cnt = static_cast<char>(argc - 3);
    std::vector<std::string> files;
    for (int i = 3; i < argc; i++) {
      files.push_back(std::string(argv[i]));
    }
    if (!write_cmd(session_id, STORE, file_cnt, files)) {
      printf("Failed to send store cmd to daemon. :(\n");
      return 1;
    }
    if (!read_err(session_id, err, msg)) {
      printf("Failed to receive result from daemon. :(\n");
      return 1;
    }
    if (static_cast<bool>(err)) {
      printf("Error occurred in daemon while processing command. :(\n");
    }
    printf(msg.c_str());
  } else if (cmd == "load") {
    if (argc < 5) {
      printf("Provide session id for a running founder client, at least one file to download, and a file to write to.\n");
      return 0;
    }
    int session_id = std::stoi(argv[2]);
    char file_cnt = static_cast<char>(argc - 3);
    std::vector<std::string> files;
    for (int i = 3; i < argc; i++) {
      files.push_back(std::string(argv[i]));
    }
    if (!write_cmd(session_id, LOAD, file_cnt, files)) {
      printf("Failed to send load cmd to daemon. :(\n");
      return 1;
    }
    if (!read_err(session_id, err, msg)) {
      printf("Failed to receive result from daemon. :(\n");
      return 1;
    }
    if (static_cast<bool>(err)) {
      printf("Error occurred in daemon while processing command. :(\n");
    }
    printf(msg.c_str());
  } else if (cmd == "exit") {
    if (argc < 3) {
      printf("Provide session id for a running founder client.\n");
      return 0;
    }
    int session_id = std::stoi(argv[2]);
    if (!write_cmd(session_id, EXIT, 0, std::vector<std::string>())) {
      printf("Failed to send exit cmd to daemon. :(\n");
      return 1;
    }
    remove_daemon_files(session_id);
  } else {
    printf("Command not found. Enter `founder help` for all commands.\n");
  }
  return 0;
}
