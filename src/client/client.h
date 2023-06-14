#include <session.h>
#include <utils.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <string>
#include <streambuf>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <chrono>
#include <filesystem>

// DAEMON
#define SESSION_DIR(session_id) (std::filesystem::path("daemons") / std::to_string(session_id))
#define LOGS_FILE(session_id) (SESSION_DIR(session_id) / "logs")
#define CMD_FILE(session_id) (SESSION_DIR(session_id) / "cmd")
#define ERR_FILE(session_id) (SESSION_DIR(session_id) / "err")

void setup_daemon();
void run_founder_session_daemon(int startup_client_id, std::vector<std::string> endpoints);
bool read_cmd(int& client_id, char& cmd, char& argc, std::vector<std::string>& args);
bool write_err(int client_id, char err, std::string msg);
bool write_cmd(int session_id, char cmd, char argc, std::vector<std::string> args);
bool read_err(int session_id, char& err, std::string& msg);
bool setup_client();
bool add_daemon_files(int session_id);
void remove_daemon_files(int session_id);
std::string get_home_dir();

// CMD
enum CMD {
  HELP = 0,
  START = 1,
  EXIT = 2,
  LIST = 3,
  STORE = 4,
  LOAD = 5,
};
struct client_state {
  bool dying;
  std::vector<std::string> endpoints;
  std::vector<Session*> sessions;
  std::vector<std::thread*> background_threads;
  std::string cmd_err;
  std::string cmd_out;
  session_metadata* meta;
};
std::string help_cmd();
bool bootstrap_cmd(client_state& state, std::vector<std::string> endpoints);
bool start_background_strain_reliever_cmd(client_state& state);
bool join_cmd(client_state& state, std::string my_endpoint, std::string ext_endpoint);
void exit_cmd(client_state& state);
bool list_cmd(client_state& state);
bool store_cmd(client_state& state, std::vector<std::string> files);
bool load_cmd(client_state& state, std::vector<std::string> input_files, std::string output_file);

// FILES
bool init_index_file(Session* s);
bool add_files_to_index_file(Session* s, std::vector<std::string> files);
bool get_index_files(Session* s, std::vector<std::string>& files_buffer);
bool write_from_file(Session* s, std::string file, std::string dht_filename);
bool read_in_files(Session* s, std::vector<std::string> files, std::vector<char>** file_data_buffer);
bool file_exists(Session* s, std::string dht_filename);
