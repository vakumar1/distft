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

  // start server RPC threads running in background
  this->init_server(self_endpoint);
  this->init_rpc_threads();

  // generate self's key and get the initial peer's key
  Key self_key = random_key();
  Peer other_peer = {random_key(), init_endpoint};

  while (!this->ping(&other_peer, &other_peer));
  this->router = new Router(self_key, self_endpoint, other_peer.key, other_peer.endpoint);

  // perform a node lookup on self
  this->self_lookup(self_key);
  
}

Session::~Session() {
  // set dying to true to invalidate all peer/chunk data for RPCs
  this->dying = true;

  // re-assign all chunks to other peers by republishing
  for (auto& pair : this->chunks) {
    Key chunk_key = pair.first;
    Chunk* chunk = pair.second;
    std::deque<Peer*> closest_peers;
    this->router->closest_peers(chunk_key, PEER_LOOKUP_ALPHA, closest_peers);
    for (Peer* other_peer : closest_peers) {
      this->store(other_peer, chunk_key, chunk->data, chunk->size);
    }
  }

  // stop all RPC threads
  this->shutdown_server();
  this->shutdown_rpc_threads();

  // memory cleanup: destroy all chunks and router
  for (auto& pair : this->chunks) {
    Key key = pair.first;
    Chunk* chunk = pair.second;
    delete chunk;
  }

  delete this->router;

}

void Session::set(Key search_key, std::byte* data, size_t size) {
  this->node_lookup(search_key);

  // select the ALPHA closest keys to store the chunk
  std::deque<Peer*> closest_peers;
  this->router->closest_peers(search_key, PEER_LOOKUP_ALPHA, closest_peers);
  Chunk* chunk = new Chunk(data, size);
  Dist max_dist;
  max_dist.value.reset();
  for (Peer* other_peer : closest_peers) {
    this->store(other_peer, chunk->key, chunk->data, chunk->size);
    max_dist = std::max(max_dist, Dist(chunk->key, other_peer->key));
  }

  // figure out whether key should also be set locally
  if (max_dist >= Dist(chunk->key, this->router->get_self_peer()->key)) {
    this->chunks[chunk->key] = chunk;
  } else {
    delete chunk;
  }
}

bool Session::get(Key search_key, std::byte** data_buffer, size_t* size_buffer) {
  // check if the key is cached locally
  if (this->chunks.count(search_key) > 0) {
    Chunk* found_chunk = this->chunks[search_key];
    *data_buffer = new std::byte[found_chunk->size];
    std::memcpy(*data_buffer, found_chunk->data, found_chunk->size);
    *size_buffer = found_chunk->size;
    return true;
  }
  return this->value_lookup(search_key, data_buffer);
  
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

      // send async find node RPC to get K closest keys the peer knows of
      // to refresh the Router
      this->find_node(other_peer, self_key);
      queried.insert(other_peer->key);
    }

    // send pings to all peers except its closest key (to add self to their routers)
    std::deque<Peer*> peers;
    this->router->all_peers(peers);
    this->router->closest_peers(self_key, 1, closest_peers);
    Key closest_key = closest_peers.at(0)->key;
    for (Peer* other_peer : closest_peers) {
      if (other_peer->key == closest_key) {
        continue;
      }
      Peer actual_peer;
      this->ping(other_peer, &actual_peer);
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

      // send async find node RPC to get K closest keys the peer knows of
      // to refresh the Router
      this->find_node(other_peer, node_key);
      queried.insert(other_peer->key);
    }
  }

  // synchronously send final find node RPCs to the K closest nodes
  this->router->closest_peers(node_key, KBUCKET_MAX, closest_peers);
  for (Peer* other_peer : closest_peers) {
    
    // send synchronous find node RPC to get K closest keys the peer knows of
    // to refresh the Router
    this->find_node(other_peer, node_key);

  }
}

bool Session::value_lookup(Key chunk_key, std::byte** data_buffer) {
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

      // send async find value RPC to get K closest keys the peer knows of
      // to refresh the Router
      // immediately return if the peer returns a value
      if(this->find_value(other_peer, chunk_key, data_buffer)) {
        return true;
      }

      queried.insert(other_peer->key);
    }
  }

  // failed to find key
  return false;
}
