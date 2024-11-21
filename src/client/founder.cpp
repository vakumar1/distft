#include "src/client/client.h"

#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <regex>

// daemon command handlers
void print_result(bool err, std::string err_msg, std::string succ_msg) {
  if (static_cast<bool>(err)) {
    std::cout << "Error occurred in daemon while processing command. :(" << std::endl;
  }
  if (!err_msg.empty()) {
    std::cout << err_msg.c_str() << std::endl;
  }
  if (!succ_msg.empty()) {
    std::cout << succ_msg.c_str() << std::endl;
  }
}

bool valid_endpoint(const std::string& ep) {
    std::regex pattern(R"((.*):(\d+))");
    std::smatch match;
    return std::regex_match(ep, match, pattern);
}

int handle_start(std::vector<std::string> endpoints) {
  pid_t client_id = getpid();
  pid_t session_id = fork();
  if (session_id < 0) {
    std::cout << "Failed to create new daemon. Aborting. :(" << std::endl;
    return INTERNAL_ERROR;
  }
  if (session_id == 0) {
    setup_daemon();
    run_founder_session_daemon(client_id, endpoints);
    return SUCCESS;
  }
  add_daemon_files(session_id);
  char err;
  std::string err_msg;
  std::string succ_msg;
  if (!read_err(session_id, err, err_msg, succ_msg)) {
    std::cout << "Failed to receive result from daemon. :(" << std::endl;
    return INTERNAL_ERROR;
  }
  std::cout << "SESSION ID: " << session_id << std::endl;
  print_result(static_cast<bool>(err), err_msg, succ_msg);
  return SUCCESS;
}

int handle_exit(int session_id) {
  if (!write_cmd(session_id, EXIT, 0, std::vector<std::string>())) {
    std::cout << "Failed to send exit cmd to daemon. :(" << std::endl;
    return INTERNAL_ERROR;
  }
  remove_daemon_files(session_id);
  return SUCCESS;
}

int handle_list(int session_id) {
  if (!write_cmd(session_id, LIST, 0, std::vector<std::string>())) {
    std::cout << "Failed to send list cmd to daemon. :(" << std::endl;
    return INTERNAL_ERROR;
  }
  char err;
  std::string err_msg;
  std::string succ_msg;
  if (!read_err(session_id, err, err_msg, succ_msg)) {
    std::cout << "Failed to receive result from daemon. :(" << std::endl;
    return INTERNAL_ERROR;
  }
  print_result(static_cast<bool>(err), err_msg, succ_msg);
  return SUCCESS;
}

int handle_store(int session_id, int file_cnt, std::vector<std::string> files) {
  if (!write_cmd(session_id, STORE, file_cnt, files)) {
    std::cout << "Failed to send store cmd to daemon. :(" << std::endl;
    return INTERNAL_ERROR;
  }
  char err;
  std::string err_msg;
  std::string succ_msg;
  if (!read_err(session_id, err, err_msg, succ_msg)) {
    std::cout << "Failed to receive result from daemon. :(" << std::endl;
    return INTERNAL_ERROR;
  }
  print_result(static_cast<bool>(err), err_msg, succ_msg);
  return SUCCESS;
}

int handle_load(int session_id, int file_cnt, std::vector<std::string> files) {
  if (!write_cmd(session_id, LOAD, file_cnt, files)) {
    std::cout << "Failed to send load cmd to daemon. :(" << std::endl;
    return INTERNAL_ERROR;
  }
  char err;
  std::string err_msg;
  std::string succ_msg;
  if (!read_err(session_id, err, err_msg, succ_msg)) {
    std::cout << "Failed to receive result from daemon. :(" << std::endl;
    return INTERNAL_ERROR;
  }
  print_result(static_cast<bool>(err), err_msg, succ_msg);
  return SUCCESS;
}

int main(int argc, char* argv[]) {
  // parse arguments for flags
  if (argc < 2) {
    std::cout << "Provide arg(s). Enter `--help` for all commands." << std::endl;
    return USER_ERROR;
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
    std::cout << daemon_commands().c_str() << std::endl;
    return SUCCESS;
  }
  if (interactive) {
    run_founder_session_interactive();
    return SUCCESS;
  }

  // DAEMON-BASED CLIENT
  // setup client and logger
  if (!setup_client()) {
    std::cout << "Failed to setup client directory in ~."  << std::endl;
    return INTERNAL_ERROR;
  }

  std::string cmd(argv[1]);
  char err;
  std::string msg;
  if (cmd == "start") {
    if (argc < 4) {
      std::cout << "Provide at least 2 accessible endpoints for the founder client." << std::endl;
      return USER_ERROR;
    }
    std::vector<std::string> endpoints;
    for (int i = 2; i < argc; i++) {
      std::string ep(argv[i]);
      if (!valid_endpoint(ep)) {
        std::cout << "Endpoint invalid: must have the form <address>:<port>" << std::endl;
        return USER_ERROR;
      }
      endpoints.push_back(std::string(argv[i]));
    }
    return handle_start(endpoints);

    
  } else if (cmd == "exit") {
    if (argc < 3) {
      std::cout << "Provide session id for a running founder client." << std::endl;
      return USER_ERROR;
    }
    std::string session_id_arg = argv[2];
    if (!std::all_of(session_id_arg.begin(), session_id_arg.end(), [](unsigned char c){ return std::isdigit(c); })
        || !std::filesystem::exists(CMD_FILE(std::stoi(session_id_arg)))) {
      std::cout << "Session ID not found. :O" << std::endl;
      return USER_ERROR;
    }
    int session_id = std::stoi(session_id_arg);
    return handle_exit(session_id);
  } else if (cmd == "list") {
    if (argc < 3) {
      std::cout << "Provide session id for a running founder client." << std::endl;
      return USER_ERROR;
    }
    std::string session_id_arg = argv[2];
    if (!std::all_of(session_id_arg.begin(), session_id_arg.end(), [](unsigned char c){ return std::isdigit(c); })
        || !std::filesystem::exists(CMD_FILE(std::stoi(session_id_arg)))) {
      std::cout << "Session ID not found. :O" << std::endl;
      return USER_ERROR;
    }
    int session_id = std::stoi(session_id_arg);
    return handle_list(session_id);
  } else if (cmd == "store") {
    if (argc < 4) {
      std::cout << "Provide session id for a running founder client and at least one file." << std::endl;
      return USER_ERROR;
    }
    std::string session_id_arg = argv[2];
    if (!std::all_of(session_id_arg.begin(), session_id_arg.end(), [](unsigned char c){ return std::isdigit(c); })
        || !std::filesystem::exists(CMD_FILE(std::stoi(session_id_arg)))) {
      std::cout << "Session ID not found. :O" << std::endl;
      return USER_ERROR;
    }
    int session_id = std::stoi(session_id_arg);
    char file_cnt = static_cast<char>(argc - 3);
    std::vector<std::string> files;
    for (int i = 3; i < argc; i++) {
      files.push_back(std::string(argv[i]));
    }
    return handle_store(session_id, file_cnt, files);
  } else if (cmd == "load") {
    if (argc < 5) {
      std::cout << "Provide session id for a running founder client, at least one file to download, and a file to write to." << std::endl;
      return USER_ERROR;
    }
    std::string session_id_arg = argv[2];
    if (!std::all_of(session_id_arg.begin(), session_id_arg.end(), [](unsigned char c){ return std::isdigit(c); })
        || !std::filesystem::exists(CMD_FILE(std::stoi(session_id_arg)))) {
      std::cout << "Session ID not found. :O" << std::endl;
      return USER_ERROR;
    }
    int session_id = std::stoi(session_id_arg);
    char file_cnt = static_cast<char>(argc - 3);
    std::vector<std::string> files;
    for (int i = 3; i < argc; i++) {
      files.push_back(std::string(argv[i]));
    }
    return handle_load(session_id, file_cnt, files);
  }
  std::cout << "Command not found. Enter `--help` for all commands." << std::endl;
  return SUCCESS;
}
