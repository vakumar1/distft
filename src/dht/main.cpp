#include "session.h"

int main() {
  #ifdef DEBUG
  spdlog::set_level(spdlog::level::debug);
  #else
  spdlog::set_level(spdlog::level::info);
  #endif
  
  std::string addr1("0.0.0.0:50051");
  std::string addr2("0.0.0.0:50053");
  Key* data_key = new Key;
  auto start_session = [data_key](std::string my_addr, std::string other_addr) {
    Session* session = new Session(my_addr, other_addr);
    std::this_thread::sleep_for(std::chrono::seconds(1500));
    std::byte* data;
    size_t size;
    bool found = session->get(*data_key, &data, &size);
    std::cout << found << std::endl;
    if (found) {
      for (int i = 0; i < size; i++) {
        std::cout << static_cast<int>(data[i]) << std::endl;
      }
    }
    delete session;
  };
  std::thread t1(start_session, addr1, addr2);
  Session* session = new Session(addr2, addr1);
  std::byte* data = new std::byte[10];
  for (int i = 0; i < 10; i++) data[i] = static_cast<std::byte>(i);
  *data_key = session->set(data, 10);
  delete session;
  t1.join();
  return 0;
}
