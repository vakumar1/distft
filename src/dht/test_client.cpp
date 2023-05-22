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
  req.set_recv_key(Key().set().to_string());

  dht::PingResponse res;
  grpc::ClientContext context;

  // Make the RPC call to the server
  grpc::Status status = stub->Ping(&context, req, &res);

  // Check if the RPC call was successful
  if (status.ok()) {
    // Process the response from the server
    std::cout << "Response: " << res.recv_key() << std::endl;
  } else {
    // Handle the error case
    std::cout << "RPC failed with error code: " << status.error_code() << std::endl;
    std::cout << "Error message: " << status.error_message() << std::endl;
  }
}
