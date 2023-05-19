#include <grpcpp/grpcpp.h>
#include "basic.pb.h"
#include "basic.grpc.pb.h"

class BasicServiceImpl final : public basic::BasicService::Service {
  grpc::Status AddOne(grpc::ServerContext* context, const basic::AddOneRequest* request,
                          basic::AddOneResponse* response) override {
    // Implement the logic for YourMethod
    int value = request->value();
    int result = value + 1;
    response->set_result(result);
    return grpc::Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  BasicServiceImpl service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main() {
  RunServer();
  return 0;
}