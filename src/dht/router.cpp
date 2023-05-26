#include "router.h"

#include <string.h>
#include <deque>
#include <algorithm>
#include <chrono>
#include <random>

//
// ROUTER
//

Router::Router(Key& self_key, std::string& self_endpoint, Key& other_key, std::string& other_endpoint) {
  // initialize self peer and table with initial peer
  this->self_peer = new Peer(self_key, self_endpoint);
  this->table = new BinaryTree(KEYBITS - 1, NULL);
  this->attempt_insert_peer(other_key, other_endpoint, NULL);
}

Router::~Router() {
  delete this->self_peer;
  delete this->table;
}

// attempt to insert the peer into the correct kbucket
// if the kbucket is full, return the LRU peer
bool Router::attempt_insert_peer(Key& peer_key, std::string endpoint, Peer** lru_peer_buffer) {
  BinaryTree* tree = this->table;
  for (int i = KEYBITS - 1; i >= 0; i--) {
    bool key_bit = peer_key[tree->split_bit_index];
    bool key_match = key_bit == this->self_peer->key[tree->split_bit_index];

    // traverse until we hit a leaf
    if (!tree->leaf) {
      tree = key_bit ? tree->one_tree : tree->zero_tree;
      continue;
    }

    // insert into bucket if it has space
    if (tree->kbucket.size() < KBUCKET_MAX) {
      tree->latest_access = std::chrono::steady_clock::now();
      Peer* peer = new Peer(peer_key, endpoint);
      tree->kbucket.push_front(peer);
      int size_diff = 1;

      // adjust key counts
      if (size_diff != 0) {
        while (tree != NULL) {
          tree->key_count += size_diff;
          tree = tree->parent;
        }
      }
      return true;
    }

    // if prefixes match (and we haven't exhausted all the bits in the key), 
    // split tree and continue traversing
    if (key_match && tree->split_bit_index > 0) {
      tree->split();
      tree = key_bit ? tree->one_tree : tree->zero_tree;
      continue;
    }

    // failed to insert key
    *lru_peer_buffer = tree->kbucket.back();
    break;
  }
  return false;
}

// evict peer from its kbucket
void Router::evict_peer(Key& evict_key) {
  BinaryTree* tree = this->table;
  for (int i = KEYBITS - 1; i >= 0; i--) {
    bool key_bit = evict_key[tree->split_bit_index];

    // traverse until we hit a leaf
    if (!tree->leaf) {
      tree = key_bit ? tree->one_tree : tree->zero_tree;
      continue;
    }

    // if peer is contained at this leaf, replace and refresh it
    for (int i = 0; i < tree->kbucket.size(); i++) {
      if (tree->kbucket.at(i)->key == evict_key) {
        tree->kbucket.erase(tree->kbucket.begin() + i); 
        return;
      }
    }
  }
}

// move a contacted peer to front of its kbucket
void Router::update_seen_peer(Key& peer_key) {
  BinaryTree* tree = this->table;
  for (int i = KEYBITS - 1; i >= 0; i--) {
    bool key_bit = peer_key[tree->split_bit_index];

    // traverse until we hit a leaf
    if (!tree->leaf) {
      tree = key_bit ? tree->one_tree : tree->zero_tree;
      continue;
    }

    // if peer is contained at this leaf, move it to the front of the kbucket
    for (int i = 0; i < tree->kbucket.size(); i++) {
      if (tree->kbucket.at(i)->key == peer_key) {
        tree->latest_access = std::chrono::steady_clock::now();
        std::rotate(tree->kbucket.begin(), tree->kbucket.begin() + i, tree->kbucket.begin() + i + 1);
        return;
      }
    }
  }

}

// return vector of (potentially less than) n closest peers to the given keys
void Router::closest_peers(Key& search_key, unsigned int n, std::deque<Peer*>& buffer) {
  this->table->tree_closest_peers(search_key, n, buffer);
}

// return all keys in the router
void Router::all_peers(std::deque<Peer*>& buffer) {
  this->table->tree_all_peers(buffer);
}

// get Peer* corresponding to the search key
// returns NULL if the router does not contain the key
Peer* Router::get_peer(Key& search_key) {
  std::deque<Peer*> buffer;
  this->closest_peers(search_key, 1, buffer);
  if (buffer.empty() || buffer.at(0)->key != search_key) {
    return NULL;
  }
  return buffer.at(0);
}

// get the peer corresponding to self
Peer* Router::get_self_peer() {
  return this->self_peer;
}

// get a random peer from each bucket that has not been accessed in the given amount of time
void Router::random_per_bucket_peers(std::deque<Peer*>& peer_buffer, std::chrono::seconds unaccessed_time) {
  BinaryTree* tree = this->table;

}

//
// BINARY TREE (HELPERS)
//

Router::BinaryTree::BinaryTree(int split_bit_index, BinaryTree* parent) {
  this->leaf = true;
  this->split_bit_index = split_bit_index;
  this->key_count = 0;
  this->latest_access = std::chrono::steady_clock::now();
  this->parent = parent;
  this->zero_tree = NULL;
  this->one_tree = NULL;
};

Router::BinaryTree::~BinaryTree() {
  // delete peers in kbucket
  if (this->leaf) {
    while (!this->kbucket.empty()) {
      Peer* peer = this->kbucket.front();
      this->kbucket.pop_front();
      delete peer;
    }
  }

  // recursively delete sub-trees
  if (this->zero_tree != NULL) {
    delete this->zero_tree;
  }
  if (this->one_tree != NULL) {
    delete this->one_tree;
  }
};

// split Binary Tree leaf in half and reassign keys into correct sub-tree
void Router::BinaryTree::split() {
  this->leaf = false;
  this->zero_tree = new BinaryTree(this->split_bit_index - 1, this);
  this->one_tree = new BinaryTree(this->split_bit_index - 1, this);
  while (this->kbucket.size() > 0) {
    Peer* peer = this->kbucket.front();
    this->kbucket.pop_front();
    bool key_bit = peer->key[this->split_bit_index];
    BinaryTree* tree = key_bit ? this->one_tree : this->zero_tree;
    tree->kbucket.push_back(peer);
    tree->key_count++;
  }
}


// get the (potentially less than) n closest keys to the key
void Router::BinaryTree::tree_closest_peers(Key& search_key, unsigned int n, std::deque<Peer*>& buffer) {
  // add at most n keys from a leaf to the buffer
  if (this->leaf) {
    this->latest_access = std::chrono::steady_clock::now();
    for (int i = 0; i < n && i < this->kbucket.size(); i++) {
      buffer.push_back(this->kbucket.at(i));
    }
    return;
  }

  // try to add n keys from the matching tree
  // add (potentially) remaining keys from the mismatched tree
  bool key_bit = search_key[this->split_bit_index];
  BinaryTree* preferred_tree = key_bit ? this->one_tree : this->zero_tree;
  BinaryTree* alternate_tree = key_bit ? this->zero_tree : this->one_tree;
  preferred_tree->tree_closest_peers(search_key, n, buffer);
  if (n < preferred_tree->key_count) {
    alternate_tree->tree_closest_peers(search_key, preferred_tree->key_count - n, buffer);
  }
}

void Router::BinaryTree::tree_all_peers(std::deque<Peer*>& buffer) {
  if (this->leaf) {
    for (int i = 0; i < this->kbucket.size(); i++) {
      buffer.push_back(this->kbucket.at(i));
    }
    return;
  }
  this->one_tree->tree_all_peers(buffer);
  this->zero_tree->tree_all_peers(buffer);
}

// add a random peer from each bucket that has not been accessed in the specified amount of time
void Router::BinaryTree::random_per_bucket_peers_helper(std::deque<Peer*>& peer_buffer, std::chrono::seconds unaccessed_time) {
  if (this->leaf) {
    this->latest_access = std::chrono::steady_clock::now();
    std::chrono::seconds time_since_access = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - this->latest_access);
    if (time_since_access >= unaccessed_time) {
      peer_buffer.push_back(this->kbucket.at(std::rand() % this->kbucket.size()));
    }
    return;
  }

  this->zero_tree->random_per_bucket_peers_helper(peer_buffer, unaccessed_time);
  this->one_tree->random_per_bucket_peers_helper(peer_buffer, unaccessed_time);
}