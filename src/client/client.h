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
void run_founder_session_daemon(int startup_client_id, std::vector<std::string> endpoints);
bool setup_daemon(int session_id);
void teardown_daemon(int session_id);
bool write_cmd_to_daemon(int session_id, char cmd, char argc, std::vector<std::string> args);
bool read_cmd_from_daemon(int& client_id, char& cmd, char& argc, std::vector<std::string>& args);
bool write_error_from_daemon(int client_id, char err, std::string msg);
bool read_error_from_daemon(int session_id, char& err, std::string& msg);
std::string get_home_dir();
void init_signal_handlers();
void wait_for_read_signal();
void read_signal_handler(int signum);

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
