#include "session.h"

//
// RPC HANDLERS
//

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

void Session::shutdown_server() {
  this->server->Shutdown();
  this->server_thread.join();
}

grpc::Status Session::FindNode(grpc::ServerContext* context, 
                            const dht::FindNodeRequest* request,
                            dht::FindNodeResponse* response) {
  
  Key search_key = Key(request->search_key());
  std::deque<Peer*> closest_keys;
  this->router->closest_peers(search_key, KBUCKET_MAX, closest_keys);
  for (Peer* peer : closest_keys) {
    response->add_closest_keys(peer->key.to_string());
  }
  return grpc::Status::OK;
}

grpc::Status Session::FindValue(grpc::ServerContext* context, 
                        const dht::FindValueRequest* request,
                        dht::FindValueResponse* response) {

  Key search_key = Key(request->search_key());
  if (this->chunks.count(search_key) > 0) {
    Chunk* found_chunk = this->chunks[search_key];
    response->mutable_data()->assign(found_chunk->data, found_chunk->data + CHUNK_BYTES);
    response->set_found_value(true);
    return grpc::Status::OK;
  }
  std::deque<Peer*> closest_keys;
  this->router->closest_peers(search_key, KBUCKET_MAX, closest_keys);
  for (Peer* peer : closest_keys) {
    response->add_closest_keys(peer->key.to_string());
  }
  response->set_found_value(false);
  return grpc::Status::OK;

}

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

grpc::Status Session::Ping(grpc::ServerContext* context, 
                        const dht::PingRequest* request,
                        dht::PingResponse* response) {
  response->set_recv_key(this->router->get_self_peer()->key.to_string());
  return grpc::Status::OK;
}

grpc::Status Session::Republish(grpc::ServerContext* context, 
                        const dht::RepublishRequest* request,
                        dht::RepublishResponse* response) {
  return grpc::Status::OK;
}
