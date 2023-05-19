#include "router.h"
#include "peer.h"

#include <unordered_map>
#include <string>

class Session {
private:
  Router router;
  std::unordered_map<Key, Peer> peers;
  std::unordered_map<Key, std::string> chunks;

public:
  // constructor: set up session and initialize routing table with initial peer
  Session(Peer& init_peer);

  // destructor: tear down session and re-distribute keys to other peers
  ~Session();

  // set key->value in DHT
  void set(Key key, std::string);

  // get value from DHT
  std::string get(Key key);
};