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
  std::deque<Peer> buffer;
  this->node_lookup(search_key, buffer);

  // select the ALPHA closest keys to store the chunk
  Chunk* chunk = new Chunk(data, size);
  Dist max_dist;
  max_dist.value.reset();
  for (int i = 0; i < PEER_LOOKUP_ALPHA && i < buffer.size(); i++) {
    Peer other_peer = buffer.at(i);
    this->store(&other_peer, chunk->key, chunk->data, chunk->size);
    max_dist = std::max(max_dist, Dist(chunk->key, other_peer.key));
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
  std::deque<Peer> buffer;
  return this->value_lookup(search_key, buffer, data_buffer);
  
}

//
// NODE LOOKUP ALGORITHMS
//

// lookup self (does not return a buffer; only populates the router)
void Session::self_lookup(Key self_key) {
  std::deque<Peer> closest_peers;
  auto self_query_fn = [this, &self_key, &closest_peers](Peer& peer) {
    this->find_node(&peer, self_key, closest_peers);
    return false;
  };
  this->lookup_helper(self_key, closest_peers, self_query_fn);

  // send pings to all peers
  std::deque<Peer*> peers;
  this->router->all_peers(peers);
  for (Peer* other_peer : peers) {
    Peer actual_peer;
    this->ping(other_peer, &actual_peer);
  }
}

// lookup a key in the DHT (populate buffer with K closest peers)
void Session::node_lookup(Key node_key, std::deque<Peer>& buffer) {
  std::deque<Peer> closest_peers;
  auto node_query_fn = [this, &node_key, &closest_peers](Peer& peer) {
    this->find_node(&peer, node_key, closest_peers);
    return false;
  };
  this->lookup_helper(node_key, closest_peers, node_query_fn);

  // synchronously send final find node RPCs to the K closest nodes
  for (Peer& other_peer : closest_peers) {
    this->find_node(&other_peer, node_key, buffer);
  }
  // return the current K closest keys
  if (buffer.size() > KBUCKET_MAX) {
    std::sort(buffer.begin(), buffer.end(), StaticDistComparator(node_key));
    buffer.resize(KBUCKET_MAX);
  }
}



// lookup a chunk in the DHT
// return true -> data_buffer is set as a pointer to the malloc'd value
// return false -> peer buffer is populated with K closest peers
bool Session::value_lookup(Key chunk_key, std::deque<Peer>& buffer, std::byte** data_buffer) {
  std::deque<Peer> closest_peers;
  bool found_value = false;
  auto value_query_fn = [this, &found_value, &chunk_key, &closest_peers, data_buffer](Peer& peer) {
    found_value = this->find_value(&peer, chunk_key, closest_peers, data_buffer);
    return found_value;
  };
  this->lookup_helper(chunk_key, closest_peers, value_query_fn);
  return found_value;
}

// perform a generic lookup on a search key
// the query function is executed on each iteration of the lookup
// and returns true if the lookup should halt
void Session::lookup_helper(Key search_key, std::deque<Peer>& closest_peers, const std::function<bool(Peer&)>& query_fn) {
  // get the current ALPHA closest keys in the router and record the min distance
  std::deque<Peer*> current_peers;
  this->router->closest_peers(search_key, PEER_LOOKUP_ALPHA, current_peers);

  // start with the ALPHA closest keys and initialize the closest distance recorded
  std::unordered_set<Key> queried;
  Dist closest_dist = Dist();
  StaticDistComparator comparator(search_key);
  for (Peer* curr_peer : current_peers) {
    closest_peers.push_back(*curr_peer);
    closest_dist = std::min(closest_dist, Dist(search_key, curr_peer->key));
  }
  for (int i = 0; i < MAX_LOOKUP_ITERS; i++) {
    // asynchronously send find node RPCs to the ALPHA closest nodes
    int lookup_count = std::min(PEER_LOOKUP_ALPHA, static_cast<int>(closest_peers.size()));
    for (int j = 0; j < lookup_count; j++) {
      Peer& other_peer = closest_peers.at(j);
      if (queried.count(other_peer.key) > 0) {
        continue;
      }

      bool halt = query_fn(other_peer);
      if (halt) {
        return;
      }
      queried.insert(other_peer.key);
    }

    // get the current K closest keys
    if (closest_peers.size() > KBUCKET_MAX) {
      std::sort(closest_peers.begin(), closest_peers.end(), comparator);
      closest_peers.resize(KBUCKET_MAX);
    }
    // halt once the distance has stopped improving
    Dist new_closest_dist = Dist(search_key, closest_peers.at(0).key);
    if (new_closest_dist >= closest_dist) {
      break;
    }
    closest_dist = new_closest_dist;
  }
}