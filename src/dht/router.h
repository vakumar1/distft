#include <utils.h>

#include <spdlog/spdlog.h>

#include <deque>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <random>

#define KBUCKET_MAX 20

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

    bool leaf;
    unsigned int split_bit_index;
    unsigned int key_count;
    std::chrono::time_point<std::chrono::system_clock> latest_access;
    BinaryTree* parent;
    BinaryTree* zero_tree;
    BinaryTree* one_tree;
    std::deque<Peer*> kbucket;

    void split();
    void tree_closest_peers(Key& search_key, unsigned int n, std::deque<Peer*>& buffer);
    void tree_all_peers(std::deque<Peer*>& buffer);
    void random_per_bucket_peers_helper(std::deque<Peer*>& peer_buffer, std::chrono::seconds unaccessed_time);
    
  };

  Peer* self_peer;
  BinaryTree* table;


  
public:
  Router(Key& self_key, std::string& self_endpoint, Key& other_key, std::string& other_endpoint);
  ~Router();

  // mutating router state
  bool attempt_insert_peer(Key& peer_key, std::string endpoint, Peer** lru_peer_buffer);
  void evict_peer(Key& evict_key);

  // accessing peers
  Peer* get_peer(Key& key);
  Peer* get_self_peer();
  void closest_peers(Key& key, unsigned int n, std::deque<Peer*>& buffer);
  void all_peers(std::deque<Peer*>& buffer);
  void random_per_bucket_peers(std::deque<Peer*>& peer_buffer, std::chrono::seconds unaccessed_time);

};

