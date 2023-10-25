#include "src/dht/session.h"

#include <vector>
#include <string>

// FILES
bool init_index_file(Session* s);
bool add_files_to_index_file(Session* s, std::vector<std::string> files);
bool get_index_files(Session* s, std::vector<std::string>& files_buffer);
bool write_from_file(Session* s, std::string file, std::string dht_filename);
bool read_in_files(Session* s, std::vector<std::string> files, std::vector<char>** file_data_buffer);
bool file_exists(Session* s, std::string dht_filename);
