#include "session.h"

#include <string>
#include <deque>
#include <unordered_set>
#include <thread>
#include <iostream>

#define MAX_LOOKUP_ITERS KEYBITS

//
// SESSION API
//

Session::Session(std::string self_endpoint, std::string init_endpoint) {
  this->dying = false;

  // generate self's key and 
  // TODO: get key from initial peer
  Key self_key = random_key();
  Key init_key = random_key();
  this->router = new Router(self_key, self_endpoint, init_key, init_endpoint);

  // TODO: start server threads running in background: 
  // RPC handler, ping RPC, republish RPC, stale chunks RPC, stale peers RPC
  this->init_server(self_endpoint);

  // perform a node lookup on self
  this->self_lookup(self_key);
  
}

Session::~Session() {
  // set dying to true to invalidate all peer/chunk data for RPCs
  this->dying = true;

  // re-assign all chunks to other peers
  for (auto& pair : this->chunks) {
    Key key = pair.first;
    Chunk chunk = *pair.second;
    // TODO: send republish RPC to peer
  }

  // TODO: stop all RPC threads
  this->shutdown_server();

  
  // memory cleanup: destroy all chunks and router
  for (auto& pair : this->chunks) {
    Key key = pair.first;
    Chunk* chunk = pair.second;
    delete chunk;
  }

  delete this->router;

}

void Session::set(Key search_key, const std::byte (&data)[CHUNK_BYTES]) {
  this->node_lookup(search_key);

  // select the ALPHA closest keys to store the chunk
  std::deque<Peer*> closest_peers;
  this->router->closest_peers(search_key, PEER_LOOKUP_ALPHA, closest_peers);
  for (Peer* other_peer : closest_peers) {
    // TODO: send async store RPC to closest peers

  }

  // TODO: figure out whether key should also be set locally
}

bool Session::get(Key search_key, std::byte (&data)[CHUNK_BYTES]) {
  // check if the key is cached locally
  if (this->chunks.count(search_key) > 0) {
    Chunk* found_chunk = this->chunks[search_key];
    std::memcpy(data, found_chunk->data, CHUNK_BYTES);
    return true;
  }
  return this->value_lookup(search_key, data);
  
}

//
// NODE LOOKUP ALGORITHMS
//

void Session::self_lookup(Key self_key) {
  std::unordered_set<Key> queried;
  std::deque<Peer*> closest_peers;
  Dist prev_dist = Dist();
  for (int i = 0; i < MAX_LOOKUP_ITERS; i++) {
    // get the current ALPHA closest keys in the router and record the min distance
    this->router->closest_peers(self_key, PEER_LOOKUP_ALPHA, closest_peers);
    Dist new_dist = Dist();
    for (Peer* other_peer : closest_peers) {
      new_dist = std::min(new_dist, Dist(self_key, other_peer->key));
    }

    // halt once the distance has stopped improving
    if (new_dist >= prev_dist) {
      break;
    }
    prev_dist = new_dist;

    // asynchronously send find node RPCs to the ALPHA closest nodes
    for (Peer* other_peer : closest_peers) {
      if (queried.count(other_peer->key) > 0) {
        continue;
      }

      // TODO: send async find node RPC to get K closest keys the peer knows of
      // to refresh the Router

      queried.insert(other_peer->key);
    }

    // send refresh requests to all peers except its closest key (to add self to their routers)
    std::deque<Peer*> peers;
    this->router->all_peers(peers);
    this->router->closest_peers(self_key, 1, closest_peers);
    Key closest_key = closest_peers.at(0)->key;
    for (Peer* other_peer : closest_peers) {
      if (other_peer->key == closest_key) {
        continue;
      }
      // TODO: send refresh RPC to peer
    }
  }
}

void Session::node_lookup(Key node_key) {
  std::unordered_set<Key> queried;
  std::deque<Peer*> closest_peers;
  Dist prev_dist = Dist();
  for (int i = 0; i < MAX_LOOKUP_ITERS; i++) {
    // get the current ALPHA closest keys in the router and record the min distance
    this->router->closest_peers(node_key, PEER_LOOKUP_ALPHA, closest_peers);
    Dist new_dist = Dist();
    for (Peer* other_peer : closest_peers) {
      new_dist = std::min(new_dist, Dist(node_key, other_peer->key));
    }

    // halt once the distance has stopped improving
    if (new_dist >= prev_dist) {
      break;
    }
    prev_dist = new_dist;

    // asynchronously send find node RPCs to the ALPHA closest nodes
    for (Peer* other_peer : closest_peers) {
      if (queried.count(other_peer->key) > 0) {
        continue;
      }

      // TODO: send async find node RPC to get K closest keys the peer knows of
      // to refresh the Router

      queried.insert(other_peer->key);
    }
  }

  // synchronously send final find node RPCs to the K closest nodes
  this->router->closest_peers(node_key, KBUCKET_MAX, closest_peers);
  for (Peer* other_peer : closest_peers) {
    
    // TODO: send synchronous find node RPC to get K closest keys the peer knows of
    // to refresh the Router

  }
}

bool Session::value_lookup(Key chunk_key, std::byte (&data)[CHUNK_BYTES]) {
  std::unordered_set<Key> queried;
  std::deque<Peer*> closest_peers;
  Dist prev_dist = Dist();
  for (int i = 0; i < MAX_LOOKUP_ITERS; i++) {
    // get the current ALPHA closest keys in the router and record the min distance
    this->router->closest_peers(chunk_key, PEER_LOOKUP_ALPHA, closest_peers);
    Dist new_dist = Dist();
    for (Peer* other_peer : closest_peers) {
      new_dist = std::min(new_dist, Dist(chunk_key, other_peer->key));
    }

    // halt once the distance has stopped improving
    if (new_dist >= prev_dist) {
      break;
    }
    prev_dist = new_dist;

    // asynchronously send find value RPCs to the ALPHA closest nodes
    for (Peer* other_peer : closest_peers) {
      if (queried.count(other_peer->key) > 0) {
        continue;
      }

      // TODO: send async find value RPC to get K closest keys the peer knows of
      // to refresh the Router
      // immediately return if the peer returns a value
      return true;

      queried.insert(other_peer->key);
    }
  }

  // failed to find key
  return false;
}
