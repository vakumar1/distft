#include "session.h"

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

// republish chunks that haven't been republished in a while by anyone
void Session::republish_chunks_thread_fn() {
  std::chrono::seconds sleep_time(10);
  std::chrono::seconds unpublished_time(3600);
  while (true) {
    std::this_thread::sleep_for(sleep_time);
    if (this->dying) {
      return;
    }
    for (const auto& pair : this->chunks) {
      Key chunk_key = pair.first;
      Chunk* chunk = pair.second;
      std::chrono::seconds time_since_publish = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - chunk->last_published);
      if (chunk->original_publisher && time_since_publish >= unpublished_time) {
        // remove chunk from local map and republish
        this->chunks.erase(chunk_key);
        this->publish(chunk);
      }
    }
  }
}

// remove expired chunks
void Session::cleanup_chunks_thread_fn() {
  std::chrono::seconds sleep_time(10);
  while (true) {
    std::this_thread::sleep_for(sleep_time);
    if (this->dying) {
      return;
    }
    for (const auto& pair : this->chunks) {
      Key chunk_key = pair.first;
      Chunk* chunk = pair.second;
      if (chunk->expiry_time >= std::chrono::steady_clock::now()) {
        // remove chunk from local map and delete
        this->chunks.erase(chunk_key);
        delete chunk;
      }
    }
  }
}

// store time since last lookup for each peer and perform random lookup on a peer
// in each bucket
void Session::refresh_peer_thread_fn() {
  std::chrono::seconds sleep_time(10);
  std::chrono::seconds unaccessed_time(3600);
  std::deque<Peer> buffer;
  while (true) {
    std::this_thread::sleep_for(sleep_time);
    if (this->dying) {
      return;
    }
    std::deque<Peer*> refresh_peers;
    this->lock.lock();
    this->router->random_per_bucket_peers(refresh_peers, unaccessed_time);
    this->lock.unlock();
    std::deque<Peer> buffer;
    for (Peer* peer : refresh_peers) {
      this->node_lookup(peer->key, buffer);
      buffer.clear();
    }
  }

}


//
// RPC HANDLERS
//

// get the K closest nodes to the provided key
grpc::Status Session::FindNode(grpc::ServerContext* context, 
                            const dht::FindNodeRequest* request,
                            dht::FindNodeResponse* response) {
  
  // update sender and set receiver
  dht::Peer sender = request->sender();
  dht::Peer* receiver = new dht::Peer;
  this->rpc_handler_prelims(&sender, receiver);
  response->set_allocated_receiver(receiver);

  // set closest keys
  Key search_key = Key(request->search_key());
  std::deque<Peer*> closest_keys;
  this->lock.lock();
  this->router->closest_peers(search_key, KBUCKET_MAX, closest_keys);
  this->lock.unlock();
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

  // update sender and set receiver
  dht::Peer sender = request->sender();
  dht::Peer* receiver = new dht::Peer;
  this->rpc_handler_prelims(&sender, receiver);
  response->set_allocated_receiver(receiver);

  Key search_key = Key(request->search_key());
  // found key -> send data
  if (this->chunks.count(search_key) > 0) {
    Chunk* found_chunk = this->chunks[search_key];
    const char* data = reinterpret_cast<const char*>(found_chunk->data);
    response->mutable_data()->assign(data, data + found_chunk->size);
    response->set_found_value(true);
    return grpc::Status::OK;
  }

  // no local chunk -> send closest keys
  std::deque<Peer*> closest_keys;
  this->lock.lock();
  this->router->closest_peers(search_key, KBUCKET_MAX, closest_keys);
  this->lock.unlock();
  for (Peer* peer : closest_keys) {
    dht::Peer* rpc_peer = response->add_closest_peers();
    rpc_peer->set_key(peer->key.to_string());
    rpc_peer->set_endpoint(peer->endpoint);
  }
  response->set_found_value(false);
  return grpc::Status::OK;

}

// tell sender if it can proceed to send data for store
grpc::Status Session::StoreInit(grpc::ServerContext* context, 
                        const dht::StoreInitRequest* request,
                        dht::StoreInitResponse* response) {

  // update sender and set receiver
  dht::Peer sender = request->sender();
  dht::Peer* receiver = new dht::Peer;
  this->rpc_handler_prelims(&sender, receiver);
  response->set_allocated_receiver(receiver);

  // continue store if data not stored locally
  Key chunk_key = Key(request->chunk_key());
  response->set_continue_store(this->chunks.count(chunk_key) == 0);
  return grpc::Status::OK;

}

// store the key/bytes pair locally
grpc::Status Session::Store(grpc::ServerContext* context, 
                        const dht::StoreRequest* request,
                        dht::StoreResponse* response) {
  // update sender and set receiver
  dht::Peer sender = request->sender();
  dht::Peer* receiver = new dht::Peer;
  this->rpc_handler_prelims(&sender, receiver);
  response->set_allocated_receiver(receiver);

  // store chunk locally
  size_t size = request->data().size();
  std::byte* data = new std::byte[size];
  std::memcpy(data, request->data().data(), size);
  long time_to_expire = request->time_to_expire();

  Chunk* chunk = new Chunk(data, size, false, std::chrono::steady_clock::now() + std::chrono::seconds(time_to_expire));
  this->chunks[chunk->key] = chunk;
  return grpc::Status::OK;
}

// refresh self's local router with peer
grpc::Status Session::Ping(grpc::ServerContext* context, 
                        const dht::PingRequest* request,
                        dht::PingResponse* response) {
  // update sender and set receiver
  dht::Peer sender = request->sender();
  dht::Peer* receiver = new dht::Peer;
  this->rpc_handler_prelims(&sender, receiver);
  response->set_allocated_receiver(receiver);

  return grpc::Status::OK;
}

//
// RPC CALLERS
//


void Session::find_node(Peer* peer, Key& search_key, std::deque<Peer>& buffer) {
  std::unique_ptr<dht::DHTService::Stub> stub = rpc_stub(peer);
  dht::FindNodeRequest request;
  grpc::ClientContext context;
  dht::FindNodeResponse response;

  // add sender and search key to request
  dht::Peer* self_peer_rpc = new dht::Peer;
  this->rpc_caller_prelims(self_peer_rpc);
  request.set_allocated_sender(self_peer_rpc);
  request.set_search_key(search_key.to_string());

  grpc::Status status = stub->FindNode(&context, request, &response);
  if (!status.ok()) {
    return;
  }

  // update receiver
  dht::Peer receiver_rpc = response.receiver();
  this->rpc_caller_epilogue(&receiver_rpc);

  // store closest keys and add to buffer
  for (dht::Peer peer : response.closest_peers()) {
    Peer local_peer;
    this->rpc_peer_to_local(&peer, &local_peer);
    this->update_peer(local_peer.key, local_peer.endpoint);
    buffer.push_back(local_peer);
   }
}

bool Session::find_value(Peer* peer, Key& search_key, std::deque<Peer>& buffer, std::byte** data_buffer) {
  std::unique_ptr<dht::DHTService::Stub> stub = rpc_stub(peer);
  dht::FindValueRequest request;
  grpc::ClientContext context;
  dht::FindValueResponse response;

  // add sender and search key to request
  dht::Peer* self_peer_rpc = new dht::Peer;
  this->rpc_caller_prelims(self_peer_rpc);
  request.set_allocated_sender(self_peer_rpc);
  request.set_search_key(search_key.to_string());

  grpc::Status status = stub->FindValue(&context, request, &response);
  if (!status.ok()) {
    return false;
  }
  
  // update receiver
  dht::Peer receiver_rpc = response.receiver();
  this->rpc_caller_epilogue(&receiver_rpc);

  // copy data into data buffer if found
  if (response.found_value()) {
    size_t size = response.data().size();
    *data_buffer = new std::byte[size];
    std::memcpy(*data_buffer, response.data().data(), size);
    return true;
  }

  // store closest keys and add to buffer
  for (dht::Peer peer : response.closest_peers()) {
    Peer local_peer;
    this->rpc_peer_to_local(&peer, &local_peer);
    this->update_peer(local_peer.key, local_peer.endpoint);
    buffer.push_back(local_peer);
   }
  return false;
}

void Session::store(Peer* peer, Key& chunk_key, std::byte* data_buffer, size_t size, std::chrono::steady_clock::duration time_to_expire) {
  // part i: initial storage request
  std::unique_ptr<dht::DHTService::Stub> stub = rpc_stub(peer);
  dht::StoreInitRequest init_request;
  grpc::ClientContext init_context;
  dht::StoreInitResponse init_response;

  // add sender and chunk key to request
  dht::Peer* self_peer_init_rpc = new dht::Peer;
  this->rpc_caller_prelims(self_peer_init_rpc);
  init_request.set_allocated_sender(self_peer_init_rpc);
  init_request.set_chunk_key(chunk_key.to_string());

  grpc::Status status = stub->StoreInit(&init_context, init_request, &init_response);
  if (!status.ok()) {
    return;
  }

  // update receiver
  dht::Peer receiver_rpc = init_response.receiver();
  this->rpc_caller_epilogue(&receiver_rpc);

  // part ii: send store data
  dht::StoreRequest request;
  grpc::ClientContext context;
  dht::StoreResponse response;

  // add sender and chunk key + data to request
  dht::Peer* self_peer_rpc = new dht::Peer;
  this->rpc_caller_prelims(self_peer_rpc);
  request.set_allocated_sender(self_peer_rpc);
  request.set_chunk_key(chunk_key.to_string());
  const char* data = reinterpret_cast<const char*>(data_buffer);
  request.mutable_data()->assign(data, data + size);
  request.set_time_to_expire(time_to_expire.count());

  status = stub->Store(&context, request, &response);
  if (!status.ok()) {
    return;
  }

  // update receiver
  this->rpc_caller_epilogue(&receiver_rpc);
}

// send a ping to a peer
// return false if the peer does not respond
bool Session::ping(Peer* peer, Peer* receiver_peer_buffer) {
  std::unique_ptr<dht::DHTService::Stub> stub = rpc_stub(peer);
  dht::PingRequest request;
  grpc::ClientContext context;
  dht::PingResponse response;
  
  // add sender to request
  dht::Peer* self_peer_rpc = new dht::Peer;
  this->rpc_caller_prelims(self_peer_rpc);
  request.set_allocated_sender(self_peer_rpc);

  grpc::Status status = stub->Ping(&context, request, &response);
  if (!status.ok()) {
    return false;
  }

  // update receiver
  dht::Peer receiver_rpc = response.receiver();
  this->rpc_caller_epilogue(&receiver_rpc);
  receiver_peer_buffer->key = Key(receiver_rpc.key());
  receiver_peer_buffer->endpoint = receiver_rpc.endpoint();
  return true;
}


//
// RPC helpers
//

// create a DHTService stub for sending RPC calls
std::unique_ptr<dht::DHTService::Stub> Session::rpc_stub(Peer* peer) {
  std::string server_address(peer->endpoint);
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
  std::unique_ptr<dht::DHTService::Stub> stub = dht::DHTService::NewStub(channel);
  return stub;
}

// convert sender/receiver peers between local/rpc formats for RPC handlers
// and update sender locally
void Session::rpc_handler_prelims(dht::Peer* sender, dht::Peer* receiver_buffer) {
  Peer local_sender;
  rpc_peer_to_local(sender, &local_sender);
  this->update_peer(local_sender.key, local_sender.endpoint);
  local_to_rpc_peer(this->router->get_self_peer(), receiver_buffer);
}

// conver sender (self) to rpc format
void Session::rpc_caller_prelims(dht::Peer* sender_buffer) {
  Peer* self_peer = this->router->get_self_peer();
  local_to_rpc_peer(self_peer, sender_buffer);
}

// convert receiver to local format and update locally
void Session::rpc_caller_epilogue(dht::Peer* receiver) {
  Peer local_receiver;
  rpc_peer_to_local(receiver, &local_receiver);
  this->update_peer(local_receiver.key, local_receiver.endpoint);
}

// convert local Peer to RPC peer
void Session::local_to_rpc_peer(Peer* peer, dht::Peer* rpc_peer_buffer) {
  std::string key_str = peer->key.to_string();
  rpc_peer_buffer->set_key(key_str);
  rpc_peer_buffer->set_endpoint(peer->endpoint);
}

// convert RPC peer to local peer
void Session::rpc_peer_to_local(dht::Peer* rpc_peer, Peer* peer_buffer) {
  peer_buffer->key = Key(rpc_peer->key());
  peer_buffer->endpoint = rpc_peer->endpoint();
}

// update the peer's position in LRU bucket (insert if not found)
void Session::update_peer(Key& peer_key, std::string endpoint) {
  
  // attempt to insert peer and evict lru peer if stale
  Peer* lru_peer;
  while(true) {
    this->lock.lock();
    bool inserted = this->router->attempt_insert_peer(peer_key, endpoint, &lru_peer);
    this->lock.unlock();
    if (inserted) {
      return;
    }
    Peer other_peer;
    bool lru_ping = this->ping(lru_peer, &other_peer);
    if (lru_ping && lru_peer->key == other_peer.key) {
      return;
    } else {
      this->lock.lock();
      this->router->evict_peer(lru_peer->key);
      this->lock.unlock();
    }
  }
}
