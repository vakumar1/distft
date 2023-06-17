#include <session.h>
#include <utils.h>

#include <functional>
#include <filesystem>

// Session integration tests
std::function<bool()> create_destroy_sessions(unsigned int num_endpoints);
std::function<bool()> store_chunks_fn(unsigned int num_chunks, unsigned int num_endpoints);
std::function<bool()> churn_chunks_fn(unsigned int num_chunks, unsigned int num_servers, unsigned int num_clients, unsigned int chunk_tol);

// file integration tests
std::function<bool()> server_static_files(unsigned int num_servers, unsigned int num_files, size_t file_size, 
                                          unsigned int found_tol, unsigned int corr_tol);
std::function<bool()> server_dynamic_files(unsigned int num_servers, unsigned int num_files, size_t file_size, 
                                            unsigned int found_tol, unsigned int corr_tol);
// utils
Chunk* random_chunk(size_t size);
void random_file(std::filesystem::path path, size_t size);
void create_session(Session*& session, unsigned int my_idx, unsigned int other_idx);
void create_chunk(Session* session, Chunk*& chunk, size_t size);
void verify_chunk(Session* s, Chunk* c, std::mutex& lock, unsigned int& ctr);
void verify_file(Session* s, std::vector<char>* file_data, std::string dht_filename, 
                  std::mutex& lock, unsigned int& found_ctr, unsigned int& corr_ctr);
void wait_on_threads(std::vector<std::thread*>& threads);