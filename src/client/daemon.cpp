#include "client.h"

// DAEMON MANAGEMENT

// global signal counter (requires that no signals can be sent before fork in start!!)
// int unhandled_signals = 0;
// std::mutex ctr_lock;
// std::condition_variable ctr_cv;

// handle requests from client as a founder session
void run_founder_session_daemon(int startup_client_id, std::vector<std::string> endpoints) {
  int session_id = getpid();
  
  // wait until log, cmd, and error pipes exist (wait for max of 1 min.)
  for (int i = 0; i < 60; i++) {
    if (std::filesystem::exists(LOGS_FILE(session_id)) &&
        std::filesystem::exists(CMD_FILE(session_id)) &&
        std::filesystem::exists(ERR_FILE(session_id))) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  std::string logger_name = std::string("logger") + std::to_string(session_id);
  auto logger = spdlog::basic_logger_mt(logger_name, LOGS_FILE(session_id));
  logger->flush_on(spdlog::level::err);

  // start the session cluster
  client_state state;
  if (!bootstrap_cmd(state, endpoints) || !start_background_strain_reliever_cmd(state)) {
    logger->info("START: failed to create new session.");
    write_err(startup_client_id, static_cast<char>(true), state.cmd_err);
    return;
  }
  logger->info("START: successfully started new session cluster.");
  write_err(startup_client_id, static_cast<char>(false), state.cmd_out);
  
  bool success;
  char cmd;
  char argc;
  std::vector<std::string> args;
  std::string output;
  while (true) {
    state.cmd_err.clear();
    state.cmd_out.clear();
    output.clear();
    int client_id;
    if (!read_cmd(client_id, cmd, argc, args)) {
      logger->error("Failed to read command from client. Skipping.\n");
      continue;
    }
    if (cmd == EXIT) {
      logger->info("EXIT: tearing down session");
      exit_cmd(state);
      exit(0);
    } else if (cmd == LIST) {
      logger->info("LIST: logging index file contents");
      success = list_cmd(state);
      output = success ? state.cmd_out : state.cmd_err;
    } else if (cmd == STORE) {
      logger->info("STORE: storing files");
      success = store_cmd(state, args);
      output = success ? state.cmd_err + state.cmd_out : state.cmd_err;
    } else if (cmd == LOAD) {
      logger->info("LOAD: loading files from session");
      std::string output_file = args.back();
      args.pop_back();
      success = load_cmd(state, args, output_file);
      output = success ? state.cmd_err + state.cmd_out : state.cmd_err;
    } else {
      logger->error("Read invalid cmd from pipe. Skipping.");
      continue;
    }
    if (!write_err(client_id, static_cast<char>(!success), output)) {
      logger->error("Failed to write error to client. Skipping.\n");
    }
  }
}

// setup the daemon
void setup_daemon() {
  setsid() < 0;

  // close all files and redirect stdin/stdout to /dev/null
  for (int i = getdtablesize(); i >= 0; i--) {
    close(i);
  }
	int i = open("/dev/null", O_RDWR);
  dup2(STDIN_FILENO, i);
  dup2(STDOUT_FILENO, i);
}

// DAEMON-SIDE COMMS HELPERS

// read cmd + arguments from the pipe in the daemon
// called from DAEMON
bool read_cmd(int& client_id, char& cmd, char& argc, std::vector<std::string>& args) {
  pid_t session_id = getpid();
  std::string logger_name = std::string("logger") + std::to_string(session_id);
  auto logger = spdlog::get(logger_name);

  // block until pipe is ready to read cmd from a client
  int pipefd = open(CMD_FILE(session_id).c_str(), O_RDONLY);
  if (read(pipefd, &client_id, sizeof(pid_t)) < 0) {
    logger->error("Pipe error reading cmd id: id={}", session_id);
    return false;
  }
  if (read(pipefd, &cmd, 1) < 0) {
    logger->error("Pipe error reading cmd id: id={}", session_id);
    return false;
  }
  if (read(pipefd, &argc, 1) < 0) {
    logger->error("Pipe error reading cmd id: id={}", session_id);
    return false;
  }
  args.clear();
  uint8_t args_count = static_cast<uint8_t>(argc);
  std::string curr_arg;
  char buff;
  while (args.size() < args_count) {
    if (read(pipefd, &buff, 1) < 0) {
      logger->error("Pipe error reading cmd id: id={}", session_id);
      return false;
    }
    if (buff == '\0') {
      if (curr_arg.length() > 0) {
        args.push_back(curr_arg);
      }
      curr_arg.clear();
    } else {
      curr_arg.push_back(buff);
    }
  }
  close(pipefd);
  return true;
}

// write cmd error
// called from DAEMON
bool write_err(int client_id, char err, std::string msg) {
  pid_t session_id = getpid();
  std::string logger_name = std::string("logger") + std::to_string(session_id);
  auto logger = spdlog::get(logger_name);

  // write error to client (or just return if client isn't ready to read)
  int pipefd = open(ERR_FILE(session_id).c_str(), O_WRONLY | O_NONBLOCK);
  if (pipefd < 0) {
    logger->error("Failed to open pipe: id={}\n", session_id);
    return false;
  }
  if (write(pipefd, &err, 1) < 0) {
    logger->error("Failed to write to client: id={}\n", session_id);
    return false;
  }
  const char* arg_str = msg.c_str();
  if (write(pipefd, arg_str, msg.length() + 1) < 0) {
    logger->error("Failed to write to client: id={}\n", session_id);
    return false;
  }
  // kill(client_id, SIGUSR1);
  close(pipefd);
  return true;
}

// CLIENT-SIDE COMMS HELPERS

// write client pid + cmd + arguments to pipe corresponding to the given session
// called from CLIENT
bool write_cmd(int session_id, char cmd, char argc, std::vector<std::string> args) {
  std::string logger_name = std::string("client_logs");
  auto logger = spdlog::get(logger_name);
  
  // open pipe and block until the daemon is ready to receive cmd
  int pipefd = open(CMD_FILE(session_id).c_str(), O_WRONLY);
  if (pipefd < 0) {
    logger->error("Failed to find running process: id={}\n", session_id);
    return false;
  }
  pid_t client_id = getpid();
  char* client_id_arr = reinterpret_cast<char*>(&client_id);
  if (!write(pipefd, client_id_arr, sizeof(pid_t))) {
    logger->error("Failed to write to daemon: id={}\n", session_id);
    return false;
  }
  if (write(pipefd, &cmd, 1) < 0) {
    logger->error("Failed to write to daemon: id={}\n", session_id);
    return false;
  }
  if (write(pipefd, &argc, 1) < 0) {
    logger->error("Failed to write to daemon: id={}\n", session_id);
    return false;
  }
  for (std::string arg : args) {
    const char* arg_str = arg.c_str();
    if (write(pipefd, arg_str, arg.length() + 1) < 0) {
      logger->error("Failed to write to daemon: id={}\n", session_id);
      return false;
    }
  }
  close(pipefd);
  return true;
}

// read the cmd error
// called from CLIENT
bool read_err(int session_id, char& err, std::string& msg) {
  std::string logger_name = std::string("client_logs");
  auto logger = spdlog::get(logger_name);

  // read error back from daemon (blocks until daemon is ready to write)
  int pipefd = open(ERR_FILE(session_id).c_str(), O_RDONLY);
  if (read(pipefd, &err, 1) < 0) {
    logger->error("Pipe error reading cmd id: id={}", session_id);
    return false;
  }
  msg.clear();
  char buff;
  while (true) {
    if (read(pipefd, &buff, 1) < 0) {
      logger->error("Pipe error reading cmd id: id={}", session_id);
      return false;
    }
    if (buff == '\0') {
      break;
    } else {
      msg.push_back(buff);
    }
  }
  return true;
}

// CLIENT-SIDE FILE MANAGEMENT

// setup the client directory at ~/.distft and chdir to client directory
bool setup_client() {
  std::filesystem::path client_dir = std::filesystem::path(get_home_dir()) / ".distft";
  if (!std::filesystem::exists(client_dir)) {
    if (!std::filesystem::create_directory(client_dir)) {
      return false;
    }
  }
  std::filesystem::path daemon_dir = client_dir / "daemons";
  if (!std::filesystem::exists(daemon_dir)) {
    if (!std::filesystem::create_directory(daemon_dir)) {
      return false;
    }
  }
  std::filesystem::path client_logs = client_dir / "client_logs";
  if (!std::filesystem::exists(client_logs)) {
    std::ofstream { client_logs };
  }
  std::filesystem::current_path(client_dir);
  std::string logger_name = std::string("client_logger");
  auto logger = spdlog::basic_logger_mt(logger_name, "client_logs");
  logger->flush_on(spdlog::level::err);
  return true;
}


// add local files for daemon to log/pipe
bool add_daemon_files(int session_id) {
  // create session directory and log/pipe files
  if (!std::filesystem::create_directory(SESSION_DIR(session_id))) {
    return false;
  }
  std::ofstream { LOGS_FILE(session_id) };
  if (mkfifo(CMD_FILE(session_id).c_str(), 0666) < 0) {
    return false;
  }
  if (mkfifo(ERR_FILE(session_id).c_str(), 0666) < 0) {
    return false;
  }
  return true;
}

// remove daemon-specific local files
void remove_daemon_files(int session_id) {
  std::filesystem::remove(LOGS_FILE(session_id));
  unlink(CMD_FILE(session_id).c_str());
  unlink(ERR_FILE(session_id).c_str());
  std::filesystem::remove(SESSION_DIR(session_id));
}



// GENERAL HELPERS

std::string get_home_dir() {
  char* home = getenv("HOME");
  if (home == NULL) {
    home = strdup("/tmp/");
  }
  return std::string(home);
}
