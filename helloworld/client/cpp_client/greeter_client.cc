#include <chrono>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <memory>
#include <string>

#include <signal.h>

#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "examples/protos/helloworld.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientWriter;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;
using helloworld::FloatNumberList;
using helloworld::FloatNumber;
using helloworld::IntNumber;
using helloworld::LongString;
using namespace std;

class GreeterClient {
 public:
  GreeterClient(shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  string SayHello(const string& user) {
    // Data we are sending to the server.
    HelloRequest request;
    request.set_name(user);

    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->SayHello(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      cout << status.error_code() << ": " << status.error_message()
                << endl;
      return "RPC failed";
    }
  }

  FloatNumber ComputeMeanRepeated(float *arr, int arr_length) {
    FloatNumberList float_arr;
    for(int i = 0; i < arr_length; i++) {
      float_arr.add_value(arr[i]);
    }
    ClientContext context;
    FloatNumber reply;
    Status status = stub_->ComputeMeanRepeated(&context, float_arr, &reply);
    return reply;
  }

  FloatNumber ComputeMean(int *arr, int arr_length) {
    ClientContext context;
    FloatNumber reply;
    
    std::unique_ptr<ClientWriter<IntNumber> > writer(
        stub_->ComputeMean(&context, &reply));

    for (int i = 0; i < arr_length; i++){
      IntNumber int_number;
      int_number.set_value(arr[i]);
      // cout << "Thread " << getpid() << " sending " << arr[i] << endl;
      if (!writer->Write(int_number)) {
        cout << "Broken stream" << endl;
        break;
      }
    }
    writer->WritesDone();
    Status status = writer->Finish();
    if (status.ok()) {
      return reply;
    } else {
      FloatNumber error;
      error.set_value(-1);
      return error;
    }
  }

  LongString SendLongString(string str) {
    ClientContext context;
    LongString lng_str;
    lng_str.set_str(str);
    LongString reply;
    // cout << "Thread " << getpid() << " sending " << str << endl;

    Status status = stub_->SendLongString(&context, lng_str, &reply);
    return reply;
  }

 private:
  unique_ptr<Greeter::Stub> stub_;
};

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint specified by
  // the argument "--target=" which is the only expected argument.
  // We indicate that the channel isn't authenticated (use of
  // InsecureChannelCredentials()).
  string target_str;
  string arg_str("--target");
  if (argc > 1) {
    string arg_val = argv[1];
    size_t start_pos = arg_val.find(arg_str);
    if (start_pos != string::npos) {
      start_pos += arg_str.size();
      if (arg_val[start_pos] == '=') {
        target_str = arg_val.substr(start_pos + 1);
      } else {
        cout << "The only correct argument syntax is --target="
                  << endl;
        return 0;
      }
    } else {
      cout << "The only acceptable argument is --target=" << endl;
      return 0;
    }
  } else {
    target_str = "localhost:50051";
  }
  
  pid_t C1 = fork();
  if (C1 == 0){
    GreeterClient greeter(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
    int arr[3] = {1, 2, 3};
    FloatNumber mean = greeter.ComputeMean(arr, 3);
    // cout << "Thread " << getpid() << " got mean: " << mean.value() << endl;
  }
  else{
    pid_t C2 = fork();
    if (C2 == 0){
      GreeterClient greeter(
        grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
      // std::this_thread::sleep_for(std::chrono::milliseconds(2));
      // kill(C1, SIGSTOP);
      LongString lng_str = greeter.SendLongString("aaaaaaaaaa");
      // cout << "Thread " << getpid() << " got long string: " << lng_str.str() << endl;
    }
    else {
      // waitpid(C2, NULL, WUNTRACED);
      // kill(C1, SIGCONT);
    }
  }

  return 0;
}
