#include "key.h"

#include <deque>
#include <unordered_map>

#define KBUCKET_MAX 5

// Router: stores all key->peer mappings in an unbalanced binary tree that allows easy
// querying of "close" peers
// Kademlia uses k1 ^ k2 to measure distance between any two keys
// practically keys are grouped by the least significant set bit of key ^ other (i.e., the highest i
// such that k1[i] != k2[i])
class Router {
private:
  
  // BinaryTree: stores keys at different levels based on their "distance" to the router's key
  // Each node in the binary tree is either a
  // (i) joint: splits into 2 sub-trees each corresponding to adding bit 0 or 1 to the matching prefix
  // (ii) leaf: contains a kbucket (LRU-ish cache) with at most K keys that have a common 
  //      least signficant set bit of key ^ other
  class BinaryTree {    

  public:
    BinaryTree(int split_bit_index, BinaryTree* parent);
    ~BinaryTree();

    // split Binary Tree leaf in half and reassign keys into correct sub-tree
    void split();

    // get the (potentially less than) n closest keys to the key
    void tree_closest_peers(Key& key, unsigned int n, std::deque<Peer*>& buffer);

    // get all peers in the tree
    void tree_all_peers(std::deque<Peer*>& buffer);

    bool leaf;
    unsigned int split_bit_index;
    unsigned int key_count;
    BinaryTree* parent;
    BinaryTree* zero_tree;
    BinaryTree* one_tree;
    std::deque<Peer*> kbucket;
    
  };

  Peer* self_peer;
  BinaryTree* table;

  
public:
  Router(Key& self_key, std::string& self_endpoint, Key& other_key, std::string& other_endpoint);
  ~Router();

  // attempt to add key into correct kbucket in table
  // the insertion may fail if the designated kbucket is full
  void insert_peer(Key& peer_key, std::string endpoint);

  void refresh_peer(Key& peer_key);

  // remove the key from the table
  void remove_peer(Key& peer_key);

  // return vector of (potentially less than) n closest peers to the given keys
  void closest_peers(Key& key, unsigned int n, std::deque<Peer*>& buffer);

  // return all keys in the router
  void all_peers(std::deque<Peer*>& buffer);

  Peer* get_peer(Key& key);

  Peer* get_self_peer();

};

