#include <filesystem>
#include <vector>
#include <string>

// INTERACTIVE
int run_founder_session_interactive();
std::string interactive_commands();

// DAEMON
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
  client_state();
  ~client_state();
  std::string get_cmd_err();
  std::string get_cmd_out();
  void clear_cmd();

  struct client_state_data;
  client_state_data* data;
};
bool bootstrap_cmd(client_state& state, std::vector<std::string> endpoints);
bool start_background_strain_reliever_cmd(client_state& state);
bool join_cmd(client_state& state, std::string my_endpoint, std::string ext_endpoint);
void exit_cmd(client_state& state);
bool list_cmd(client_state& state);
bool store_cmd(client_state& state, std::vector<std::string> files);
bool load_cmd(client_state& state, std::vector<std::string> input_files, std::string output_file);

