#include <grpcpp/grpcpp.h>
#include "dht.pb.h"
#include "dht.grpc.pb.h"
#include "key.h"

#include <string>

int main() {
  std::string server_address("localhost:50051");
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
  std::unique_ptr<dht::DHTService::Stub> stub = dht::DHTService::NewStub(channel);

  dht::PingRequest req;
  dht::Peer sender_rpc;
  Peer sender;
  sender_rpc.set_key(sender.key.to_string());
  sender_rpc.set_endpoint(sender.endpoint);
  req.set_allocated_sender(&sender_rpc);

  dht::PingResponse res;
  grpc::ClientContext context;

  // Make the RPC call to the server
  grpc::Status status = stub->Ping(&context, req, &res);

  // Check if the RPC call was successful
  if (status.ok()) {
    // Process the response from the server
    std::cout << "Response: " << res.receiver().key() << std::endl;
  } else {
    // Handle the error case
    std::cout << "RPC failed with error code: " << status.error_code() << std::endl;
    std::cout << "Error message: " << status.error_message() << std::endl;
  }
}
