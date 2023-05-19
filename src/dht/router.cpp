#include "router.h"
#include "key.h"

#include <string.h>
#include <deque>

//
// ROUTER
//

Router::Router(Key& self_key, Key& other_key) {
  this->key = self_key;
  this->table = new BinaryTree(0, NULL);
  this->insert_key(other_key);
}

Router::~Router() {
  delete this->table;
}

void Router::insert_key(Key& key) {
  bool found = true;
  BinaryTree* tree = this->table;
  while (true) {
    bool key_bit = key.test(tree->split_bit_index);
    bool key_match = key_bit == this->key.test(tree->split_bit_index);

    // traverse until we hit a leaf
    if (!tree->leaf) {
      tree = key_bit ? tree->one_tree : tree->zero_tree;
      continue;
    }

    // insert into bucket if it has space
    if (tree->kbucket.size() < KBUCKET_MAX) {
      break;
    }

    // if prefixes match, split tree and continue traversing
    if (key_match && tree->split_bit_index < KEYBITS) {
      tree->split();
      tree = key_bit ? tree->one_tree : tree->zero_tree;
      continue;
    }

    // failed to insert key
    found = false;
    break;
  }

  // insert and increment key counts if an open kbucket was found
  if (found) {
    tree->kbucket.push_back(key);
    while (tree != NULL) {
      tree->key_count++;
      tree = tree->parent;
    }
  }
}

void Router::closest_keys(Key& key, unsigned int n, std::deque<Key>& buffer) {
  this->table->closest_keys(key, n, buffer);
}

//
// BINARY TREE (HELPERS)
//

Router::BinaryTree::BinaryTree(int split_bit_index, BinaryTree* parent) {
  this->leaf = true;
  this->split_bit_index = split_bit_index;
  this->key_count = 0;
  this->parent = parent;
  this->zero_tree = NULL;
  this->one_tree = NULL;
};

Router::BinaryTree::~BinaryTree() {
  if (this->zero_tree != NULL) {
    delete this->zero_tree;
  }
  if (this->one_tree != NULL) {
    delete this->one_tree;
  }
};


void Router::BinaryTree::split() {
  this->leaf = false;
  this->zero_tree = new BinaryTree(this->split_bit_index + 1, this);
  this->one_tree = new BinaryTree(this->split_bit_index + 1, this);
  while (this->kbucket.size() > 0) {
    Key key = this->kbucket.front();
    this->kbucket.pop_front();
    bool key_bit = key.test(this->split_bit_index + 1);
    BinaryTree* tree = key_bit ? this->one_tree : this->zero_tree;
    tree->kbucket.push_back(key);
  }
}

void Router::BinaryTree::closest_keys(Key& key, unsigned int n, std::deque<Key>& buffer) {
  // add at most n keys from a leaf to the buffer
  if (this->leaf) {
    for (int i = 0; i < n && i < this->kbucket.size(); i++) {
      buffer.push_back(this->kbucket.at(i));
    }
    return;
  }

  // try to add n keys from the matching tree
  // add (potentially) remaining keys from the mismatched tree
  bool key_bit = key.test(this->split_bit_index);
  BinaryTree* preferred_tree = key_bit ? this->one_tree : this->zero_tree;
  BinaryTree* alternate_tree = key_bit ? this->zero_tree : this->one_tree;
  preferred_tree->closest_keys(key, n, buffer);
  if (n < preferred_tree->key_count) {
    alternate_tree->closest_keys(key, preferred_tree->key_count - n, buffer);
  }
}