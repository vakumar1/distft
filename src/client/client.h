#include <session.h>
#include <utils.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <string>
#include <streambuf>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <chrono>
#include <filesystem>

// DAEMON
void run_founder_session_daemon(std::vector<std::string> endpoints);
bool setup_daemon(int session_id);
void teardown_daemon(int session_id);
bool write_cmd_to_daemon(int session_id, char cmd, char argc, std::vector<std::string> args);
bool read_cmd_from_daemon(char& cmd, char& argc, std::vector<std::string>& args);
bool write_error_from_daemon(char err, std::string msg);
bool read_error_from_daemon(int session_id, char& err, std::string& msg);
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
std::string help_cmd();
bool start_cmd(std::vector<Session*>& sessions, std::vector<std::string> endpoints, std::string& msg);
void exit_cmd(std::vector<Session*> sessions);
bool list_cmd(std::vector<Session*> sessions, std::string& msg);
bool store_cmd(std::vector<Session*> sessions, std::vector<std::string> files, std::string& msg);
bool load_cmd(std::vector<Session*> sessions, std::vector<std::string> input_files, std::string output_file, std::string& msg);

// FILES
bool init_index_file(Session* s);
bool add_files_to_index_file(Session* s, std::vector<std::string> files);
bool get_index_files(Session* s, std::vector<std::string>& files_buffer);
bool write_from_file(Session* s, std::string file);
bool read_in_files(Session* s, std::vector<std::string> files, std::vector<char>** file_data_buffer);