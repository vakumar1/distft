#include <grpcpp/grpcpp.h>
#include "dht.pb.h"
#include "dht.grpc.pb.h"

class SessionRPC : public dht::DHTService::Service{
private:
  // RPC handlers
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


};