#include "session.h"

#include <chrono>

//
// Session threads
//

// start running the server RPC handler thread
void Session::init_server(std::string server_address) {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(this);
  this->server = builder.BuildAndStart();
  this->server_thread = std::thread(&Session::handler_thread_fn, this);
}

void Session::handler_thread_fn() {
  this->server->Wait();
}

// shutdown the RPC handler and wait for it to exit
void Session::shutdown_server() {
  this->server->Shutdown();
  this->server_thread.join();
}

// start running the RPC calling threads
void Session::init_rpc_threads() {
  std::thread* republish_thread = new std::thread(&Session::republish_chunks_thread_fn, this);
  std::thread* expired_chunks_thread = new std::thread(&Session::cleanup_chunks_thread_fn, this);
  std::thread* refresh_thread = new std::thread(&Session::refresh_peer_thread_fn, this);
  this->rpc_threads.push_back(republish_thread);
  this->rpc_threads.push_back(expired_chunks_thread);
  this->rpc_threads.push_back(refresh_thread);
}

// wait for running RPC threads to exit
void Session::shutdown_rpc_threads() {
  while (this->rpc_threads.size() > 0) {
    std::thread* rpc_thread = this->rpc_threads.front();
    this->rpc_threads.pop_front();
    rpc_thread->join();
    delete rpc_thread;
  }
}

void Session::republish_chunks_thread_fn() {
  std::chrono::seconds sleep(3600);
  while (true) {
    std::this_thread::sleep_for(sleep);
    for (const auto& pair : this->chunks) {
      Key chunk_key = pair.first;
      Chunk* chunk = pair.second;
      std::deque<Peer*> closest_peers;
      this->router->closest_peers(chunk_key, PEER_LOOKUP_ALPHA, closest_peers);
      for (Peer* peer : closest_peers) {
        this->republish(peer, chunk_key, chunk->data);
      }
    }
  }
}

void Session::cleanup_chunks_thread_fn() {
  // TODO: store expiry time in chunks and remove expired chunks
}

void Session::refresh_peer_thread_fn() {
  std::chrono::seconds sleep(3600);
  // TODO: store time since last lookup for each peer and perform random lookup on a peer
  // in each bucket
}


//
// RPC HANDLERS
//

// get the K closest nodes to the provided key
grpc::Status Session::FindNode(grpc::ServerContext* context, 
                            const dht::FindNodeRequest* request,
                            dht::FindNodeResponse* response) {
  
  Key search_key = Key(request->search_key());
  std::deque<Peer*> closest_keys;
  this->router->closest_peers(search_key, KBUCKET_MAX, closest_keys);
  for (Peer* peer : closest_keys) {
    dht::Peer* rpc_peer = response->add_closest_peers();
    rpc_peer->set_key(peer->key.to_string());
    rpc_peer->set_endpoint(peer->endpoint);
  }
  return grpc::Status::OK;
}

// get the value stored (or the K closest nodes) at the key
grpc::Status Session::FindValue(grpc::ServerContext* context, 
                        const dht::FindValueRequest* request,
                        dht::FindValueResponse* response) {

  Key search_key = Key(request->search_key());
  if (this->chunks.count(search_key) > 0) {
    Chunk* found_chunk = this->chunks[search_key];
    const char* data = reinterpret_cast<const char*>(found_chunk->data);
    response->mutable_data()->assign(data, data + CHUNK_BYTES);
    response->set_found_value(true);
    return grpc::Status::OK;
  }
  std::deque<Peer*> closest_keys;
  this->router->closest_peers(search_key, KBUCKET_MAX, closest_keys);
  for (Peer* peer : closest_keys) {
    dht::Peer* rpc_peer = response->add_closest_peers();
    rpc_peer->set_key(peer->key.to_string());
    rpc_peer->set_endpoint(peer->endpoint);
  }
  response->set_found_value(false);
  return grpc::Status::OK;

}

// store the key/bytes pair locally
grpc::Status Session::Store(grpc::ServerContext* context, 
                        const dht::StoreRequest* request,
                        dht::StoreResponse* response) {
  
  Key chunk_key = Key(request->chunk_key());
  std::byte* data = new std::byte[CHUNK_BYTES];
  std::memcpy(data, request->data().data(), CHUNK_BYTES);
  Chunk* chunk = new Chunk(chunk_key, data);
  this->chunks[chunk_key] = chunk;
  return grpc::Status::OK;
}

// refresh self's local router with peer
grpc::Status Session::Ping(grpc::ServerContext* context, 
                        const dht::PingRequest* request,
                        dht::PingResponse* response) {
  Key other_key = Key(request->send_key());
  this->router->refresh_peer(other_key);
  response->set_recv_key(this->router->get_self_peer()->key.to_string());
  return grpc::Status::OK;
}

// republish a key (occurs periodically or during peer death)
// only continue to send data if the chunk is not stored locally
grpc::Status Session::RepublishInit(grpc::ServerContext* context, 
                        const dht::RepublishInitRequest* request,
                        dht::RepublishInitResponse* response) {

  Key chunk_key = Key(request->chunk_key());
  bool transfer_data = this->chunks.count(chunk_key) == 0;
  response->set_continue_republish(transfer_data);
  return grpc::Status::OK;
}

// receive and store chunk data for republish event
grpc::Status Session::RepublishData(grpc::ServerContext* context, 
                          const dht::RepublishDataRequest* request,
                          dht::RepublishDataResponse* response) {

  Key chunk_key = Key(request->chunk_key());
  std::byte* data = new std::byte[CHUNK_BYTES];
  std::memcpy(data, request->data().data(), CHUNK_BYTES);
  Chunk* chunk = new Chunk(chunk_key, data);
  this->chunks[chunk_key] = chunk;
  return grpc::Status::OK;
}


//
// RPC CALLERS
//
std::unique_ptr<dht::DHTService::Stub> rpc_stub(Peer* peer) {
  std::string server_address(peer->endpoint);
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
  std::unique_ptr<dht::DHTService::Stub> stub = dht::DHTService::NewStub(channel);
  return stub;
}

void Session::find_node(Peer* peer, Key& search_key, std::deque<Key> buffer) {
  std::unique_ptr<dht::DHTService::Stub> stub = rpc_stub(peer);
  dht::FindNodeRequest request;
  request.set_search_key(search_key.to_string());
  grpc::ClientContext context;
  dht::FindNodeResponse response;
  grpc::Status status = stub->FindNode(&context, request, &response);
  if (status.ok()) {
    for (dht::Peer rpc_peer : response.closest_peers()) {
      Key key = Key(rpc_peer.key());
      std::string endpoint = rpc_peer.endpoint();
      if (this->router->get_peer(key) == NULL) {
        this->router->insert_peer(key, endpoint);
      } else {
        this->router->refresh_peer(key);
      }
      buffer.push_back(key);
    }
  }
}

bool Session::find_value(Peer* peer, Key& search_key, std::byte** data_buffer, std::deque<Key> buffer) {
  std::unique_ptr<dht::DHTService::Stub> stub = rpc_stub(peer);
  dht::FindValueRequest request;
  request.set_search_key(search_key.to_string());
  grpc::ClientContext context;
  dht::FindValueResponse response;
  grpc::Status status = stub->FindValue(&context, request, &response);
  if (status.ok()) {
    if (response.found_value()) {
      *data_buffer = new std::byte[CHUNK_BYTES];
      std::memcpy(*data_buffer, response.data().data(), CHUNK_BYTES);
      return true;
    }
    for (dht::Peer rpc_peer : response.closest_peers()) {
      Key key = Key(rpc_peer.key());
      std::string endpoint = rpc_peer.endpoint();
      if (this->router->get_peer(key) == NULL) {
        this->router->insert_peer(key, endpoint);
      } else {
        this->router->refresh_peer(key);
      }
      buffer.push_back(key);
    }
    return false;
  }
  return false;
}

void Session::store(Peer* peer, Key& chunk_key, std::byte* data_buffer) {
  std::unique_ptr<dht::DHTService::Stub> stub = rpc_stub(peer);
  dht::StoreRequest request;
  request.set_chunk_key(chunk_key.to_string());
  const char* data = reinterpret_cast<const char*>(data_buffer);
  request.mutable_data()->assign(data, data + CHUNK_BYTES);
  grpc::ClientContext context;
  dht::StoreResponse response;
  grpc::Status status = stub->Store(&context, request, &response);
  if (status.ok()) {
    bool store_success = response.store_success();
  }
}

void Session::ping(Peer* peer) {
  std::unique_ptr<dht::DHTService::Stub> stub = rpc_stub(peer);
  dht::PingRequest request;
  request.set_send_key(this->router->get_self_peer()->key.to_string());
  grpc::ClientContext context;
  dht::PingResponse response;
  grpc::Status status = stub->Ping(&context, request, &response);
  if (status.ok()) {
    Key recv_key = Key(response.recv_key());
    if (recv_key == peer->key) {
      this->router->refresh_peer(peer->key);
    } else {
      this->router->remove_peer(peer->key);
    }
  }
}

void Session::republish(Peer* peer, Key& chunk_key, std::byte* data_buffer) {
  std::unique_ptr<dht::DHTService::Stub> stub = rpc_stub(peer);
  dht::RepublishInitRequest request;
  request.set_chunk_key(chunk_key.to_string());
  grpc::ClientContext context;
  dht::RepublishInitResponse response;
  grpc::Status status = stub->RepublishInit(&context, request, &response);
  if (status.ok()) {
    if (response.continue_republish()) {
      dht::RepublishDataRequest request;
      request.set_chunk_key(chunk_key.to_string());
      const char* data = reinterpret_cast<const char*>(data_buffer);
      request.mutable_data()->assign(data, data + CHUNK_BYTES);
      grpc::ClientContext context;
      dht::RepublishDataResponse response;
      grpc::Status status = stub->RepublishData(&context, request, &response);
      if (status.ok()) {
        bool success = response.republish_success();
      }
    }
  }
}
