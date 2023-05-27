#include "session.h"

#include <bitset>
#include <thread>

int main() {
  std::string addr1("0.0.0.0:50051");
  std::string addr2("0.0.0.0:50053");
  auto start_session = [](std::string my_addr, std::string other_addr) {
    Session* session = new Session(my_addr, other_addr);
    delete session;
  };
  std::thread t1(start_session, addr1, addr2);
  Session* session = new Session(addr2, addr1);

  delete session;
  t1.join();
  return 0;
}
