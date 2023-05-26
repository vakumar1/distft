#include "router.h"
#include "chunk.h"

#include <grpcpp/grpcpp.h>
#include "dht.pb.h"
#include "dht.grpc.pb.h"

#include <unordered_map>
#include <string>
#include <deque>
#include <thread>

#define PEER_LOOKUP_ALPHA 3

// Session: represents the local state of a peer that has joined a global session
// with at least one other peer (set at initialization)
// the Session is a wrapper around a Router (that stores other peers' key info)
// and (i) exposes the peer's RPC "server" for pings, peer lookups, and chunk requests,
// and (ii) exposes a simple get/set API for the underlying DHT implemented by 
// sending RPCs for peer lookups and chunk requests

class Session : public dht::DHTService::Service {
private:

  bool dying;
  Router* router;
  std::unordered_map<Key, Chunk*> chunks;
  std::unique_ptr<grpc::Server> server;
  std::thread server_thread;
  std::deque<std::thread*> rpc_threads;
  
  // node lookup algorithms
  void publish(Chunk* chunk);
  void self_lookup(Key self_key);
  void node_lookup(Key node_key, std::deque<Peer>& buffer);
  bool value_lookup(Key chunk_key, std::deque<Peer>& buffer, std::byte** data_buffer);
  void lookup_helper(Key search_key, std::deque<Peer>& closest_peers, const std::function<bool(Peer&)>& query_fn);

  // RPC handlers
  void init_server(std::string server_address);
  void shutdown_server();
  void handler_thread_fn();
  grpc::Status FindNode(grpc::ServerContext* context, 
                          const dht::FindNodeRequest* request,
                          dht::FindNodeResponse* response) override;
  grpc::Status FindValue(grpc::ServerContext* context, 
                          const dht::FindValueRequest* request,
                          dht::FindValueResponse* response) override;
  grpc::Status StoreInit(grpc::ServerContext* context, 
                          const dht::StoreInitRequest* request,
                          dht::StoreInitResponse* response) override;
  grpc::Status Store(grpc::ServerContext* context, 
                          const dht::StoreRequest* request,
                          dht::StoreResponse* response) override;
  grpc::Status Ping(grpc::ServerContext* context, 
                          const dht::PingRequest* request,
                          dht::PingResponse* response) override;
  
  // RPC caller threads: republish + expired chunks, refresh nodes
  void init_rpc_threads();
  void shutdown_rpc_threads();
  void republish_chunks_thread_fn();
  void cleanup_chunks_thread_fn();
  void refresh_peer_thread_fn();
  void find_node(Peer* peer, Key& search_key, std::deque<Peer>& buffer);
  bool find_value(Peer* peer, Key& search_key, std::deque<Peer>& buffer, std::byte** data_buffer);
  void store(Peer* peer, Key& chunk_key, std::byte* data_buffer, size_t size, std::chrono::steady_clock::duration time_to_expire);
  bool ping(Peer* peer, Peer* receiver_peer_buffer);

  // helpers
  std::unique_ptr<dht::DHTService::Stub> rpc_stub(Peer* peer);
  void rpc_handler_prelims(dht::Peer* sender, dht::Peer* receiver_buffer);
  void rpc_caller_prelims(dht::Peer* sender);
  void rpc_caller_epilogue(dht::Peer* receiver_buffer);
  void update_peer(Key& peer_key, std::string endpoint);
  void store_discovered_peer(Key& peer_key, std::string endpoint);
  void local_to_rpc_peer(Peer* peer, dht::Peer* rpc_peer_buffer);
  void rpc_peer_to_local(dht::Peer* rpc_peer, Peer* peer_buffer);


public:
  // constructor: set up session and initialize routing table with initial peer
  Session(std::string self_endpoint, std::string init_endpoint);

  // destructor: tear down session and re-distribute keys to other peers
  ~Session();

  // add chunk data to DHT
  Key set(std::byte* data, size_t size);

  // get value from DHT
  // returns false if key was not found
  bool get(Key search_key, std::byte** data_buffer, size_t* size_buffer);
};
