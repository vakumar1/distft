#include "session.h"

#include <bitset>

int main() {
  std::string server_address("0.0.0.0:50051");
  std::string other_address("0.0.0.0:50053");
  Session* session = new Session(server_address, other_address);

  std::string input;
  std::cout << "Enter a string: ";
  std::getline(std::cin, input);

  delete session;
  return 0;
}
