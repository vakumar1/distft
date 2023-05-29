#ifndef KEY_H
#define KEY_H

#include <string>
#include <bitset>
#include <random>
#include <iostream>
#include <sstream>

#define KEYBITS 160

//
// Keys and Distances
//

// key for chunks and peers
typedef std::bitset<KEYBITS> Key;
Key random_key();
std::string hex_string(Key k);

// represents a distance between two keys
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


//
// Peers and Chunks
//

// stores peer-specific information
struct Peer {
  Peer() {
    this->key = Key().reset();
    this->endpoint = "0.0.0.0:80";
  }
  Peer(Key key, std::string endpoint) {
    this->key = key;
    this->endpoint = endpoint;
  }
  std::string endpoint;
  Key key;
};

// compares two peers with respect to a static key
struct StaticDistComparator {
  StaticDistComparator(Key k) {
    this->key = k;
  }
  bool operator()(Peer& p1, Peer& p2) {
    return Dist(key, p1.key) < Dist(key, p2.key);
  }
  Key key;
};

#endif