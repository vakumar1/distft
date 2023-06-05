#include <session.h>

#include <spdlog/spdlog.h>

#include <assert.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <mutex>
#include <condition_variable>

// Router unit tests
bool test_router_insert();

// Session integration tests
std::function<bool()> create_destroy_sessions(unsigned int num_endpoints);
std::function<bool()> store_chunks_fn(unsigned int num_chunks, unsigned int num_endpoints);
std::function<bool()> churn_chunks_fn(unsigned int num_chunks, unsigned int num_servers, unsigned int num_clients, unsigned int chunk_tol);
