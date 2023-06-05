#include "tests.h"

bool test_router_insert() {
  Key k1 = random_key();
  Key k2 = random_key();;
  Key k3 = k2;
  k3[KEYBITS - 1].flip();

  std::string e1 = "1";
  std::string e2 = "2";
  std::string e3 = "3";
  Router router(k1, e1, k2, e2);
  router.attempt_insert_peer(k3, e3, NULL);
  std::deque<Peer*> peers;
  router.all_peers(peers);
  return peers.size() == 2;
}

