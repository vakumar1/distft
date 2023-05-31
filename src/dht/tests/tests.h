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
bool test_store_few_sessions1();
bool test_store_few_sessions2();
bool test_store_many_sessions1();
bool test_store_many_sessions2();
bool test_store_many_sessions3();

// Session helpers
bool create_destroy_sessions(unsigned int num_endpoints);
bool store_chunks(unsigned int num_chunks, unsigned int num_endpoints);
Chunk* random_chunk(size_t size);
