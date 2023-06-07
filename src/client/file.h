#include <session.h>

#include <spdlog/spdlog.h>

#include <string>
#include <streambuf>
#include <fstream>

bool init_index_file(Session* s);
bool add_files_to_index_file(Session* s, std::vector<std::string> files);
bool get_index_files(Session* s, std::vector<std::string>& files_buffer);
bool write_from_file(Session* s, std::string file);
bool read_in_file(Session* s, std::string file, std::vector<char>** file_data_buffer);
