#include "session.h"

//
// SESSION API
//

Key Session::self_key() {
  return this->router->get_self_peer()->key;
}

std::string Session::self_endpoint() {
  return this->router->get_self_peer()->endpoint;
}

void Session::startup(session_metadata* parent_metadata, std::string self_endpoint, std::string init_endpoint) {
  this->dying = false;
  this->meta = parent_metadata;

  // generate self's key and get the initial peer's key (temporarily create router)
  Key self_key = random_key();
  spdlog::debug("{} CREATING SESSION", hex_string(self_key));
  Key temp_key = random_key();
  Peer other_peer = {temp_key, init_endpoint};
  this->router_lock.lock();
  this->router = new Router(self_key, self_endpoint, other_peer.key, other_peer.endpoint);
  this->router_lock.unlock();

  // start server RPC threads running in background
  this->init_server(self_endpoint);
  this->init_rpc_threads();  

  // ping peer for correct key (and remove dummy peer from router)
  while (!this->ping(&other_peer, &other_peer));
  this->router_lock.lock();
  Peer* dummy_peer;
  this->router->attempt_insert_peer(other_peer.key, other_peer.endpoint, &dummy_peer);
  this->router->evict_peer(temp_key);
  this->router_lock.unlock();

  // perform a node lookup on self
  this->self_lookup(self_key);
}

void Session::teardown(bool republish) {
  // set dying to true to invalidate all peer/chunk data for RPCs
  this->dying = true;

  // stop all RPC threads
  this->shutdown_server();
  this->shutdown_rpc_threads();

  spdlog::debug("{} DELETING SESSION", hex_string(this->self_key()));

  // re-assign all chunks to other peers by republishing
  this->chunks_lock.lock();
  if (republish) {
    for (auto& pair : this->chunks) {
      Key chunk_key = pair.first;
      Chunk* chunk = pair.second;
      std::deque<Peer*> closest_peers;
      this->router_lock.lock();
      this->router->closest_peers(chunk_key, PEER_LOOKUP_ALPHA, closest_peers);
      this->router_lock.unlock();
      bool stored = false;
      for (Peer* other_peer : closest_peers) {
        stored = stored || this->store(other_peer, chunk);
      }
      if (!stored) {
        spdlog::error("{} DROPPED CHUNK (NOT ENOUGH PEERS): CHUNK={}", hex_string(this->self_key()), hex_string(chunk_key));
      }
    }
  } else {
    spdlog::error("{} DROPPING ALL CHUNKS", hex_string(this->self_key()));
  }

  // memory cleanup: destroy all chunks and router
  for (auto& pair : this->chunks) {
    Key key = pair.first;
    Chunk* chunk = pair.second;
    delete chunk;
  }
  this->chunks_lock.unlock();

  delete this->router;
}

// publish a new chunk of data to the DHT
void Session::set(Key key, std::vector<char>* data) {
  Chunk* chunk = new Chunk(key, data, true, std::chrono::system_clock::now());
  this->publish(chunk);
}

// publish a (new or old) chunk to the DHT
// the chunk (and its data) may be deleted if it does not
// need to be stored locally
void Session::publish(Chunk* chunk) {
  Key chunk_key = chunk->key;
  spdlog::debug("{} PUBLISH: CHUNK_KEY={}", hex_string(this->self_key()), 
                hex_string(chunk_key));
  std::deque<Peer> buffer;
  this->node_lookup(chunk_key, buffer);

  // select the ALPHA closest keys to store the chunk
  Dist max_dist;
  max_dist.value.reset();
  for (int i = 0; i < KBUCKET_MAX && i < buffer.size(); i++) {
    Peer other_peer = buffer.at(i);
    this->store(&other_peer, chunk);
    max_dist = std::max(max_dist, Dist(chunk->key, other_peer.key));
  }

  // figure out whether key should also be set locally
  if (buffer.size() <= KBUCKET_MAX || max_dist >= Dist(chunk->key, this->self_key())) {
    this->chunks_lock.lock();
    this->chunks[chunk->key] = chunk;
    this->chunks_lock.unlock();
  } else {
    delete chunk;
  }
}

bool Session::get(Key search_key, std::vector<char>** data_buffer) {
  // check if the key is cached locally
  this->chunks_lock.lock();
  if (this->chunks.count(search_key) > 0) {
    spdlog::debug("{} GET (LOCAL): CHUNK_KEY={}", hex_string(this->self_key()), 
                hex_string(search_key));
    Chunk* found_chunk = this->chunks[search_key];
    *data_buffer = new std::vector<char>(found_chunk->data->begin(), found_chunk->data->end());
    this->chunks_lock.unlock();
    return true;
  }
  this->chunks_lock.unlock();
  std::deque<Peer> buffer;
  return this->value_lookup(search_key, buffer, data_buffer);
  
}

//
// NODE LOOKUP ALGORITHMS
//

// lookup self (does not return a buffer; only populates the router)
void Session::self_lookup(Key self_key) {
  spdlog::debug("{} SELF LOOKUP", hex_string(self_key));
  std::deque<Peer> closest_peers;
  auto self_query_fn = [this, &self_key, &closest_peers]
                        (Peer& peer, std::mutex& ctr_lock, unsigned int& status_ctr) {
    if (this->find_node(&peer, self_key, closest_peers)) {
      ctr_lock.lock();
      status_ctr++;
      ctr_lock.unlock();
    }
    return false;
  };
  this->lookup_helper(self_key, closest_peers, self_query_fn);

  // send refreshes to all peers
  std::deque<Peer*> peers;
  this->router_lock.lock();
  this->router->random_per_bucket_peers(peers, std::chrono::seconds(0));
  this->router_lock.unlock();
  for (Peer* other_peer : peers) {
    std::deque<Peer> dummy_buffer;
    this->node_lookup(other_peer->key, dummy_buffer);
  }
}

// lookup a key in the DHT (populate buffer with K closest peers)
void Session::node_lookup(Key node_key, std::deque<Peer>& buffer) {
  spdlog::debug("{} NODE LOOKUP", hex_string(this->self_key()));
  std::deque<Peer> closest_peers;
  auto node_query_fn = [this, &node_key, &closest_peers]
                        (Peer& peer, std::mutex& ctr_lock, unsigned int& status_ctr) {
    if (this->find_node(&peer, node_key, closest_peers)) {
      ctr_lock.lock();
      status_ctr++;
      ctr_lock.unlock();
    }
    return false;
  };
  this->lookup_helper(node_key, closest_peers, node_query_fn);

  // synchronously send final find node RPCs to the K closest nodes
  size_t lookup_count = closest_peers.size();
  for (int i = 0; i < lookup_count; i++) {
    this->find_node(&closest_peers[i], node_key, closest_peers);
  }

  // sort and return the K closest (unique) peers
  std::unordered_set<Key> seen_peers;
  for (Peer& other_peer : closest_peers) {
    if (seen_peers.count(other_peer.key) > 0) {
      continue;
    }
    buffer.push_back(other_peer);
    seen_peers.insert(other_peer.key);
  }
  std::sort(buffer.begin(), buffer.end(), StaticDistComparator(node_key));
  if (buffer.size() > KBUCKET_MAX) {
    buffer.resize(KBUCKET_MAX);
  }
}



// lookup a chunk in the DHT
// return true -> data_buffer is set as a pointer to the malloc'd value
// return false -> peer buffer is populated with K closest peers
bool Session::value_lookup(Key chunk_key, std::deque<Peer>& buffer, std::vector<char>** data_buffer) {
  spdlog::debug("{} VALUE LOOKUP: CHUNK={}", hex_string(this->self_key()), hex_string(chunk_key));
  std::deque<Peer> closest_peers;
  bool found_value = false;
  auto value_query_fn = [this, &found_value, &chunk_key, &closest_peers, data_buffer]
                        (Peer& peer, std::mutex& ctr_lock, unsigned int& status_ctr) {
    if (this->find_value(&peer, chunk_key, &found_value, closest_peers, data_buffer)) {
      ctr_lock.lock();
      status_ctr++;
      ctr_lock.unlock();
    }
    return found_value;
  };
  this->lookup_helper(chunk_key, closest_peers, value_query_fn);
  return found_value;
}

// perform a generic lookup on a search key
// the query function is executed on each iteration of the lookup
// and returns true if the lookup should halt
void Session::lookup_helper(Key search_key, std::deque<Peer>& closest_peers, const std::function<bool(Peer&, std::mutex&, unsigned int&)>& query_fn) {
  // get the current ALPHA closest keys in the router and record the min distance
  std::deque<Peer*> local_peers;
  this->router_lock.lock();
  this->router->closest_peers(search_key, KBUCKET_MAX, local_peers);
  this->router_lock.unlock();

  // start with the ALPHA closest keys and initialize the closest distance recorded
  std::unordered_set<Key> queried;
  Dist closest_peers_min_dist = Dist();
  size_t closest_peers_size = closest_peers.size();
  StaticDistComparator comparator(search_key);
  for (Peer* curr_peer : local_peers) {
    closest_peers.push_back(*curr_peer);
    Dist curr_dist = Dist(search_key, curr_peer->key);
    closest_peers_min_dist = std::min(closest_peers_min_dist, curr_dist);
  }
  std::sort(closest_peers.begin(), closest_peers.end(), comparator);
  for (int i = 0; i < MAX_LOOKUP_ITERS; i++) {
    // asynchronously send find node RPCs to the ALPHA closest nodes
    std::mutex ctr_lock;
    unsigned int lookup_ctr = 0;
    int lookup_count = std::min(PEER_LOOKUP_ALPHA, static_cast<int>(closest_peers.size()));

    for (int j = 0; j < closest_peers.size(); j++) {
      ctr_lock.lock();
      bool done = lookup_ctr >= lookup_count;
      ctr_lock.unlock();
      if (done) {
        break;
      }

      Peer& other_peer = closest_peers[j];
      if (queried.count(other_peer.key) > 0) {
        continue;
      }

      bool halt = query_fn(other_peer, ctr_lock, lookup_ctr);
      if (halt) {
        return;
      }
      queried.insert(other_peer.key);
    }

    // get the current K (unique) closest keys
    std::unordered_set<Key> seen_peers;
    std::deque<Peer> unique_closest_peers;
    for (Peer& other_peer : closest_peers) {
      if (seen_peers.count(other_peer.key) > 0) {
        continue;
      }
      unique_closest_peers.push_back(other_peer);
      seen_peers.insert(other_peer.key);
    }
    closest_peers = unique_closest_peers;
    std::sort(closest_peers.begin(), closest_peers.end(), comparator);
    if (closest_peers.size() > KBUCKET_MAX) {
      closest_peers.resize(KBUCKET_MAX);
    }

    // halt once the distance has stopped improving and < kbucket peers were found
    size_t new_closest_peers_size = closest_peers.size();
    Dist new_closest_peers_min_dist = Dist(search_key, closest_peers.at(0).key);
    if (new_closest_peers_size <= closest_peers_size && new_closest_peers_min_dist >= closest_peers_min_dist) {
      spdlog::debug("{} TERMINATE LOOKUP (NO DIST IMPROVEMENT): SEARCH_KEY={}", 
                      hex_string(this->self_key()), hex_string(search_key));
      return;
    }
    closest_peers_size = new_closest_peers_size;
    closest_peers_min_dist = new_closest_peers_min_dist;
  }
}
