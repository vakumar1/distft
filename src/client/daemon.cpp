#include "src/client/client.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// DAEMON MANAGEMENT

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
  CommandControl ctrl = CommandControl();
  if (!ctrl.bootstrap_cmd(endpoints) || !ctrl.start_background_strain_reliever_cmd()) {
    logger->info("START: failed to create new session.");
    write_err(startup_client_id, static_cast<char>(true), ctrl.get_cmd_err(), ctrl.get_cmd_out());
    return;
  }
  logger->info("START: successfully started new session cluster.");
  write_err(startup_client_id, static_cast<char>(false), ctrl.get_cmd_err(), ctrl.get_cmd_out());
  
  bool success;
  char cmd;
  char argc;
  std::vector<std::string> args;
  while (true) {
    ctrl.clear_cmd();
    int client_id;
    if (!read_cmd(client_id, cmd, argc, args)) {
      logger->error("Failed to read command from client. Skipping.\n");
      continue;
    }
    if (cmd == EXIT) {
      logger->info("EXIT: tearing down session");
      ctrl.exit_cmd();
      exit(0);
    } else if (cmd == LIST) {
      logger->info("LIST: logging index file contents");
      success = ctrl.list_cmd();
    } else if (cmd == STORE) {
      logger->info("STORE: storing files");
      success = ctrl.store_cmd(args);
    } else if (cmd == LOAD) {
      logger->info("LOAD: loading files from session");
      std::string output_file = args.back();
      args.pop_back();
      success = ctrl.load_cmd(args, output_file);
    } else {
      logger->error("Read invalid cmd from pipe. Skipping.");
      continue;
    }
    if (!write_err(client_id, static_cast<char>(!success), ctrl.get_cmd_err(), ctrl.get_cmd_out())) {
      logger->error("Failed to write error to client. Skipping.\n");
    }
  }
}

// handle requests from client as a transient session
void run_transient_session_daemon(int startup_client_id, std::string remote_endpoint, std::string local_endpoint) {
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

  // join the session cluster
  CommandControl ctrl = CommandControl();
  if (!ctrl.join_cmd(local_endpoint, remote_endpoint)) {
    logger->info("START: failed to join session.");
    write_err(startup_client_id, static_cast<char>(true), ctrl.get_cmd_err(), ctrl.get_cmd_out());
    return;
  }
  logger->info("START: successfully started new session cluster.");
  write_err(startup_client_id, static_cast<char>(false), ctrl.get_cmd_err(), ctrl.get_cmd_out());
  
  bool success;
  char cmd;
  char argc;
  std::vector<std::string> args;
  while (true) {
    ctrl.clear_cmd();
    int client_id;
    if (!read_cmd(client_id, cmd, argc, args)) {
      logger->error("Failed to read command from client. Skipping.\n");
      continue;
    }
    if (cmd == EXIT) {
      logger->info("EXIT: tearing down session");
      ctrl.exit_cmd();
      exit(0);
    } else if (cmd == LIST) {
      logger->info("LIST: logging index file contents");
      success = ctrl.list_cmd();
    } else if (cmd == LOAD) {
      logger->info("LOAD: loading files from session");
      std::string output_file = args.back();
      args.pop_back();
      success = ctrl.load_cmd(args, output_file);
    } else {
      logger->error("Read invalid cmd from pipe. Skipping.");
      continue;
    }
    if (!write_err(client_id, static_cast<char>(!success), ctrl.get_cmd_err(), ctrl.get_cmd_out())) {
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
bool write_err(int client_id, char err, std::string err_msg, std::string succ_msg) {
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
  const char* arg_str = err_msg.c_str();
  if (write(pipefd, arg_str, err_msg.length() + 1) < 0) {
    logger->error("Failed to write to client: id={}\n", session_id);
    return false;
  }
  arg_str = succ_msg.c_str();
  if (write(pipefd, arg_str, succ_msg.length() + 1) < 0) {
    logger->error("Failed to write to client: id={}\n", session_id);
    return false;
  }
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
bool read_err(int session_id, char& err, std::string& err_msg, std::string& succ_msg) {
  std::string logger_name = std::string("client_logs");
  auto logger = spdlog::get(logger_name);

  // read error back from daemon (blocks until daemon is ready to write)
  int pipefd = open(ERR_FILE(session_id).c_str(), O_RDONLY);
  if (read(pipefd, &err, 1) < 0) {
    logger->error("Pipe error reading cmd id: id={}", session_id);
    return false;
  }
  char buff;
  while (true) {
    if (read(pipefd, &buff, 1) < 0) {
      logger->error("Pipe error reading cmd id: id={}", session_id);
      return false;
    }
    if (buff == '\0') {
      break;
    } else {
      err_msg.push_back(buff);
    }
  }
  while (true) {
    if (read(pipefd, &buff, 1) < 0) {
      logger->error("Pipe error reading cmd id: id={}", session_id);
      return false;
    }
    if (buff == '\0') {
      break;
    } else {
      succ_msg.push_back(buff);
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

std::string daemon_commands() {
  return R"(
  -i/--interactive: run in interactive mode
  -h/--help: print all commands
  start <endpoint 1> ... <endpoint n>: create a new session cluster with at least 2 endpoints (prints out the session id)
  list <session id>: print all files
  [RESTRICTED TO FOUNDERS] store <session id> <file path 1> ... <file path n>: add file(s) at local file path(s) to session
  load <session id> <file name 1> ... <file name n> <file path>: write the content of file name(s) to local file path
  exit <session id>: exit the session
)";
}

std::string get_home_dir() {
  char* home = getenv("HOME");
  if (home == NULL) {
    home = strdup("/tmp/");
  }
  return std::string(home);
}
