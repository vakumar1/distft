#ifndef KEY_H
#define KEY_H

#include <string>
#include <bitset>
#include <random>
#include <iostream>

#define KEYBITS 160

//
// Keys and Distances
//

typedef std::bitset<KEYBITS> Key;
struct Dist {
  Dist() {
    value = std::bitset<KEYBITS>().set();
  };
  Dist(Key k1, Key k2) {
    value = k1 ^ k2;
  };
  std::bitset<KEYBITS> value;
  const bool operator < (const Dist& d) const;
  const bool operator >= (const Dist& d) const;
};

// generate a random 160-bit key
Key random_key();


//
// Peers and Chunks
//

struct Peer {
  Peer(Key key, std::string endpoint) {
    this->key = key;
    this->endpoint = endpoint;
  }
  std::string endpoint;
  Key key;
};

#endif