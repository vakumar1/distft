#include <dht/session.h>

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
bool test_create_few_sessions();
bool test_create_many_sessions();
std::function<bool()> store_chunks_fn(unsigned int num_chunks, unsigned int num_endpoints);
std::function<bool()> churn_chunks_fn(unsigned int num_chunks, unsigned int num_servers, unsigned int num_clients);

