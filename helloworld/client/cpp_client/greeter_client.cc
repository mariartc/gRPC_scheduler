#include <grpcpp/grpcpp.h>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <vector>

#ifdef BAZEL_BUILD
#include "examples/protos/helloworld.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientWriter;
using grpc::Status;
using helloworld::FloatNumberList;
using helloworld::FloatNumber;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;
using helloworld::IntNumber;
using helloworld::LongString;

using namespace std;

#define TCP_PORT    35001
#define HOSTNAME    "localhost"

int sd;
char buf[100];
struct hostent *hp;
struct sockaddr_in sa;

class GreeterClient {
  public:
    GreeterClient(shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

    string SayHello(const string& user) {
      HelloRequest request;
      request.set_name(user);

      HelloReply reply;
      ClientContext context;
      
      Status status = stub_->SayHello(&context, request, &reply);

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
      ClientContext context;
      FloatNumber reply;

      for(int i = 0; i < arr_length; i++) 
        float_arr.add_value(arr[i]);
      
      Status status = stub_->ComputeMeanRepeated(&context, float_arr, &reply);

      return reply;
    }


    FloatNumber ComputeMean(int *arr, int arr_length) {
      ClientContext context;
      FloatNumber reply;
      
      std::unique_ptr<ClientWriter<IntNumber> > writer(
          stub_->ComputeMean(&context, &reply));
      cout << getpid() << " starts sending array" << endl;

      for (int i = 0; i < arr_length; i++){
        IntNumber int_number;
        int_number.set_value(arr[i]);
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
      
      cout << getpid() << " sending " << str << endl;

      Status status = stub_->SendLongString(&context, lng_str, &reply);
      return reply;
    }

  private:
    unique_ptr<Greeter::Stub> stub_;
};


void write_to_socket(string str){
    str.copy(buf, str.length());
    buf[str.length()] = '\0';

	  ssize_t n;
		n = write(sd, buf, sizeof(buf));

    if(n < 0){
      cout << getpid() << " something went wrong with writing "
            << str << " to socket" << endl;
      return;
    }

    cout << getpid() << " wrote " << buf << " to socket" << endl;
}


void run_rpc(int r, string long_str, int *arr, int arr_length) {
  GreeterClient greeter(
    grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
  cout << getpid() << " going to sleep..." << endl;
  // Wait for scheduler to notify
  raise(SIGSTOP);
  cout << getpid() << " woke up! " << endl;
  if (r == 0){
      LongString lng_str = greeter.SendLongString(long_str);
      cout << getpid() << " got long string: " << lng_str.str() << endl;
  }
  else if (r == 1){
    FloatNumber mean = greeter.ComputeMean(arr, arr_length);
    cout << getpid() << " got mean: " << mean.value() << endl;
  }
}


void run_processes(int scheduler_pid){
  string str = "hello";
  int priority;
  int arr_length = 1000;
  int arr[arr_length];
  fill_n(arr, arr_length, 3);

  pid_t C1, C2;
  C1 = fork();
  if (C1 == 0){
    run_rpc(1, "", arr, arr_length);
  }
  else {
    // Wait fot C1 to fall asleep
    waitpid(C1, NULL, WUNTRACED);
    // Subscribe C1 to scheduler
    priority = 1;
    write_to_socket("subscribe" + to_string(C1) + ":" + to_string(priority));
    // Send signal to scheduler to read the pipe
    kill(scheduler_pid, SIGUSR1);
    // Wait a little so that the first rpc starts sending
    // in order to observe the switching between processes
    this_thread::sleep_for(std::chrono::milliseconds(10));
    C2 = fork();
    if (C2 == 0){
      run_rpc(0, str, NULL, -1);
    }
    else{
      // Wait fot C2 to fall asleep
      waitpid(C2, NULL, WUNTRACED);
      // Subscribe C2 to scheduler
      priority = 2;
      write_to_socket("subscribe" + to_string(C2) + ":" + to_string(priority));
      // Send signal to scheduler to read the pipe
      kill(scheduler_pid, SIGUSR1);

      // Wait for a process to terminate so that
      // the scheduler removes its priority and pid
      pid_t F1 = wait(NULL);
      write_to_socket("remove" + to_string(F1));
      kill(scheduler_pid, SIGUSR1);
      F1 = wait(NULL);
      write_to_socket("remove" + to_string(F1));
      kill(scheduler_pid, SIGUSR1);
    }
  }
}


bool connect_to_socket(){
  // Create TCP/IP socket
  if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return false;
	}
  cout << getpid() << " created TCP socket\n";

	// Look up remote hostname on DNS
	if ( !(hp = gethostbyname(HOSTNAME))) {
    cout << getpid() << " failed DNS lookup for host " << HOSTNAME << endl;
		return false;
	}

	/* Connect to remote TCP port */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
  cout << getpid() << " connecting to remote host..." << endl;
	if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("connect");
		return false;
	}
  cout << getpid() << " connected!" << endl;
  return true;
}


int main(int argc, char** argv) {
  if(argc == 1 || atoi(argv[1]) <= 0){
		cout << "Usage: ./greeter_client <scheduler_pid>\n";
		exit(0);
	}

  if(not connect_to_socket())
    exit(1);

  int scheduler_pid = atoi(argv[1]);
  run_processes(scheduler_pid);
}
