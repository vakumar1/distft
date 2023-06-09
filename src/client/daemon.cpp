#include "client.h"

// DAEMON MANAGEMENT

// handle requests from client as a founder session
void run_founder_session_daemon(std::vector<std::string> endpoints) {
  int session_id = getpid();
  std::string logger_name = std::string("logger") + std::to_string(session_id);
  std::vector<Session*> sessions;
  bool success;
  std::string msg;
  
  // wait until log, cmd, and error pipes exist
  std::string home_dir = get_home_dir();  
  std::string cmd_pipe = home_dir + "/.distft/cmd/" + std::to_string(session_id);
  std::string err_pipe = home_dir + "/.distft/err/" + std::to_string(session_id);
  std::string log_file = home_dir + "/.distft/logs/" + std::to_string(session_id);
  bool ready = false;
  for (int i = 0; i < 60; i++) {
    if (std::filesystem::exists(cmd_pipe) &&
        std::filesystem::exists(err_pipe) &&
        std::filesystem::exists(log_file)) {
      ready = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  if (!ready) {
    return;
  }
  auto logger = spdlog::basic_logger_mt(logger_name, log_file);
  logger->flush_on(spdlog::level::err);
  logger->info("START: successfully started new session cluster.");

  // start the session cluster
  if (!start_cmd(sessions, endpoints, msg)) {
    write_error_from_daemon(static_cast<char>(true), msg);
    return;
  }
  write_error_from_daemon(static_cast<char>(false), std::string());
  
  
  char cmd;
  char argc;
  std::vector<std::string> args;
  while (true) {
    if (!read_cmd_from_daemon(cmd, argc, args)) {
      logger->error("Failed to read command from client. Skipping.\n");
      continue;
    }
    if (cmd == EXIT) {
      logger->info("EXIT: tearing down session");
      exit_cmd(sessions);
      exit(0);
    } else if (cmd == LIST) {
      logger->info("LIST: logging index file contents");
      success = list_cmd(sessions, msg);
    } else if (cmd == STORE) {
      logger->info("STORE: storing files");
      success = store_cmd(sessions, args, msg);
    } else if (cmd == LOAD) {
      logger->info("LOAD: loading files from session");
      std::string output_file = args.back();
      args.pop_back();
      success = load_cmd(sessions, args, output_file, msg);
    } else {
      logger->error("Read invalid cmd from pipe. Skipping.");
      continue;
    }
    if (!write_error_from_daemon(static_cast<char>(!success), msg)) {
      logger->error("Failed to write error to client. Skipping.\n");
    }
  }
}

// create log/comms files (returns true if logs and pipe set up correctly)
bool setup_daemon(int session_id) {
  std::string home_dir = get_home_dir();
  std::string cmd_pipe = home_dir + "/.distft/cmd/" + std::to_string(session_id);
  std::string err_pipe = home_dir + "/.distft/err/" + std::to_string(session_id);
  std::string logs_file = home_dir + "/.distft/logs/" + std::to_string(session_id);
  if (creat(logs_file.c_str(), 0666) < 0) {
    return false;
  }
  if (mkfifo(cmd_pipe.c_str(), 0666) < 0) {
    return false;
  }
  if (mkfifo(err_pipe.c_str(), 0666) < 0) {
    return false;
  }
  return true;
}

// delete log/comms files and exit daemon
void teardown_daemon(int session_id) {
  std::string home_dir = get_home_dir();
  std::string logs_file = home_dir + "/.distft/logs/" + std::to_string(session_id);
  remove(logs_file.c_str());
  std::string cmd_pipe = home_dir + "/.distft/cmd/" + std::to_string(session_id);
  unlink(cmd_pipe.c_str());
  std::string err_pipe = home_dir + "/.distft/err/" + std::to_string(session_id);
  unlink(err_pipe.c_str());
  exit(0);
}

// DAEMON COMMS

// write cmd + arguments to pipe corresponding to the given session
bool write_cmd_to_daemon(int session_id, char cmd, char argc, std::vector<std::string> args) {
  std::string home_dir = get_home_dir();
  std::string cmd_pipe = home_dir + "/.distft/cmd/" + std::to_string(session_id);
  std::string logs_file = home_dir + "/.distft/logs/" + std::to_string(session_id);
  std::string logger_name = std::string("logger") + std::to_string(session_id);
  auto logger = spdlog::basic_logger_mt(logger_name, logs_file);
  logger->flush_on(spdlog::level::err);
  
  int pipefd = open(cmd_pipe.c_str(), O_WRONLY | O_NONBLOCK);
  if (pipefd < 0) {
    logger->error("Failed to find running process: id={}\n", session_id);
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

// read cmd + arguments from the pipe in the daemon
bool read_cmd_from_daemon(char& cmd, char& argc, std::vector<std::string>& args) {
  pid_t session_id = getpid();
  std::string home_dir = get_home_dir();
  std::string cmd_pipe = home_dir + "/.distft/cmd/" + std::to_string(session_id);
  std::string logger_name = std::string("logger") + std::to_string(session_id);
  auto logger = spdlog::get(logger_name);

  int pipefd = open(cmd_pipe.c_str(), O_RDONLY | O_NONBLOCK);
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(pipefd, &rfds);
  struct timeval tv;
  tv.tv_sec = 0.5;
  tv.tv_usec = 0;
  while (true) {
    int retval = select(pipefd + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1) {
      logger->error("Select error occurred: id={}", session_id);
      return false;
    }
    if (retval != 0) {
      break;
    }
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
  return true;
}

bool write_error_from_daemon(char err, std::string msg) {
  pid_t session_id = getpid();
  std::string home_dir = get_home_dir();
  std::string err_pipe = home_dir + "/.distft/err/" + std::to_string(session_id);
  std::string logger_name = std::string("logger") + std::to_string(session_id);
  auto logger = spdlog::get(logger_name);

  int pipefd = open(err_pipe.c_str(), O_WRONLY | O_NONBLOCK);
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
  close(pipefd);
  return true;
}

bool read_error_from_daemon(int session_id, char& err, std::string& msg) {
  std::string home_dir = get_home_dir();
  std::string err_pipe = home_dir + "/.distft/err/" + std::to_string(session_id);
  std::string logs_file = home_dir + "/.distft/logs/" + std::to_string(session_id);
  std::string logger_name = std::string("logger") + std::to_string(session_id);
  auto logger = spdlog::get(logger_name);

  int pipefd = open(err_pipe.c_str(), O_RDONLY | O_NONBLOCK);
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(pipefd, &rfds);
  struct timeval tv;
  tv.tv_sec = 0.5;
  tv.tv_usec = 0;
  while (true) {
    int retval = select(pipefd + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1) {
      logger->error("Select error occurred: id={}", session_id);
      return false;
    }
    if (retval != 0) {
      break;
    }
  }
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


// HELPERS

std::string get_home_dir() {
  char* home = getenv("HOME");
  if (home == NULL) {
    home = strdup("/tmp/");
  }
  return std::string(home);
}