#include "router.h"

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
  
  // node lookup algorithms
  void self_lookup(Key self_key);
  void node_lookup(Key node_key);
  Chunk value_lookup(Key chunk_key);

  // RPC handler
  void init_server(std::string server_address);
  void shutdown_server();
  void handler_thread_fn();
  grpc::Status FindNode(grpc::ServerContext* context, 
                          const dht::FindNodeRequest* request,
                          dht::FindNodeResponse* response) override;
  grpc::Status FindValue(grpc::ServerContext* context, 
                          const dht::FindValueRequest* request,
                          dht::FindValueResponse* response) override;
  grpc::Status Store(grpc::ServerContext* context, 
                          const dht::StoreRequest* request,
                          dht::StoreResponse* response) override;
  grpc::Status Ping(grpc::ServerContext* context, 
                          const dht::PingRequest* request,
                          dht::PingResponse* response) override;
  grpc::Status Republish(grpc::ServerContext* context, 
                          const dht::RepublishRequest* request,
                          dht::RepublishResponse* response) override;

  friend class DHTServiceImpl;

public:
  // constructor: set up session and initialize routing table with initial peer
  Session(std::string self_endpoint, std::string init_endpoint);

  // destructor: tear down session and re-distribute keys to other peers
  ~Session();

  // set key->value in DHT
  void set(Key key, Chunk chunk);

  // get value from DHT
  // returns empty string if key was not found
  Chunk get(Key key);
};
