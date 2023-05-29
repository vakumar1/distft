#include <dht/session.h>

#include <assert.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <chrono>

// Router unit tests
void test_router_insert();

// Session integration tests
void test_few_sessions();
void test_many_sessions();

// Session helpers
bool create_destroy_sessions(unsigned int num_endpoints);
