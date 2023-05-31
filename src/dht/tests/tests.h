#include <dht/session.h>

#include <spdlog/spdlog.h>

#include <assert.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <chrono>
#include <random>

// Router unit tests
bool test_router_insert();

// Session integration tests
bool test_create_few_sessions();
bool test_create_many_sessions();
std::function<bool()> store_chunks_fn(unsigned int num_chunks, unsigned int num_endpoints, unsigned int wait_time);

// Session helpers
bool create_destroy_sessions(unsigned int num_endpoints);
bool store_chunks(unsigned int num_chunks, unsigned int num_endpoints);
Chunk* random_chunk(size_t size);
