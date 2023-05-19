#include <grpcpp/grpcpp.h>
#include "basic.pb.h"
#include "basic.grpc.pb.h"

void RunClient() {
  std::string server_address("localhost:50051");
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
  std::unique_ptr<basic::BasicService::Stub> stub = basic::BasicService::NewStub(channel);

  basic::AddOneRequest request;
  request.set_value(13);  // Set the message field in the request

  basic::AddOneResponse response;
  grpc::ClientContext context;

  // Make the RPC call to the server
  grpc::Status status = stub->AddOne(&context, request, &response);

  // Check if the RPC call was successful
  if (status.ok()) {
    // Process the response from the server
    std::cout << "Response: " << response.result() << std::endl;
  } else {
    // Handle the error case
    std::cout << "RPC failed with error code: " << status.error_code() << std::endl;
    std::cout << "Error message: " << status.error_message() << std::endl;
  }
}

int main() {
  RunClient();
  return 0;
}