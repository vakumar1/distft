#include <filesystem>
#include <vector>
#include <string>

// INTERACTIVE API
int run_founder_session_interactive();
std::string interactive_commands();

// DAEMON API
#define SESSION_DIR(session_id) (std::filesystem::path("daemons") / std::to_string(session_id))
#define LOGS_FILE(session_id) (SESSION_DIR(session_id) / "logs")
#define CMD_FILE(session_id) (SESSION_DIR(session_id) / "cmd")
#define ERR_FILE(session_id) (SESSION_DIR(session_id) / "err")

void setup_daemon();
void run_founder_session_daemon(int startup_client_id, std::vector<std::string> endpoints);
bool read_cmd(int& client_id, char& cmd, char& argc, std::vector<std::string>& args);
bool write_err(int client_id, char err, std::string err_msg, std::string succ_msg);
bool write_cmd(int session_id, char cmd, char argc, std::vector<std::string> args);
bool read_err(int session_id, char& err, std::string& err_msg, std::string& succ_msg);
bool setup_client();
bool add_daemon_files(int session_id);
void remove_daemon_files(int session_id);
std::string get_home_dir();
std::string daemon_commands();

// CMD API
enum CMD {
  HELP = 0,
  START = 1,
  EXIT = 2,
  LIST = 3,
  STORE = 4,
  LOAD = 5,
};

class CommandControl {
private:
  // internal client state
  struct client_state_data;
  client_state_data* data;

public:
  CommandControl();
  ~CommandControl();

  // command result management
  std::string get_cmd_err();
  std::string get_cmd_out();
  void clear_cmd();

  // command execution
  bool bootstrap_cmd(std::vector<std::string> endpoints);
  bool start_background_strain_reliever_cmd();
  bool join_cmd(std::string my_endpoint, std::string ext_endpoint);
  void exit_cmd();
  bool list_cmd();
  bool store_cmd(std::vector<std::string> files);
  bool load_cmd(std::vector<std::string> input_files, std::string output_file);

};
