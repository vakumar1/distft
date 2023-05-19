#include "key.h"

#include <string>

class Peer {
public:
  std::string ip_addr;
  unsigned short port;

  Peer(std::string ip_addr, unsigned short port);
  ~Peer();
};