#include <chrono>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <signal.h>
#include <string>
#include <sys/types.h>
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

vector<int> priorities;
vector<int> pids;
int pd[2];
int max_pid, running_pid = -1;


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


int get_maximum_priority(){
  if (priorities.size() == 0) return -1;

  int max_i = -1, max = 0, n = priorities.size();
  for(int i = 0; i < n; i++){
    if (priorities[i] > max){
      max_i = i;
      max = priorities[i];
    }
  }
  return pids[max_i];
}


void schedule(){
  while(1){
    max_pid = get_maximum_priority();
    if (max_pid != -1 and max_pid != running_pid){
      if (running_pid != -1){
        kill(running_pid, SIGSTOP);
        cout << getpid() << " sent " << running_pid << " to sleep" << endl;
      }
      running_pid = max_pid;
      kill(max_pid, SIGCONT);
    }
  }
}


void subscribe_process(int priority, int pid){
  priorities.push_back(priority);
  pids.push_back(pid);
  if (priority > max_pid)
    max_pid = pid;
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
    // Subscribe C1 to scheduler
    waitpid(C1, NULL, WUNTRACED);
    priority = 1;
    write(pd[1], &C1, sizeof(int));
    write(pd[1], &priority, sizeof(int));
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
      // Subscribe C2 to scheduler
      waitpid(C2, NULL, WUNTRACED);
      priority = 2;
      write(pd[1], &C2, sizeof(int));
      write(pd[1], &priority, sizeof(int));
      // Send signal to scheduler to read the pipe
      kill(scheduler_pid, SIGUSR1);

      // Wait for a process to terminate so that
      // the scheduler removes its priority and pid
      pid_t F1 = wait(NULL);
      write(pd[1], &F1, sizeof(int));
      kill(scheduler_pid, SIGUSR2);
      F1 = wait(NULL);
      write(pd[1], &F1, sizeof(int));
      kill(scheduler_pid, SIGUSR2);
    }
  }
}


void remove_priority(int pid){
  cout << getpid() << " removing priority and pid of " << pid << endl;
  int n = pids.size();
  for(int i = 0; i < n; i++){
    if (pids[i] == pid){
      pids.erase(pids.begin() + i);
      priorities.erase(priorities.begin() + i);
      break;
    }
  }
  max_pid = get_maximum_priority();
}


// SIGUSR1 and SIGUSR2 handler
void read_from_pipe(int a){
  int priority, pid;
  if (a == SIGUSR1){
    read(pd[0], &pid, sizeof(int));
    read(pd[0], &priority, sizeof(int));
    cout << getpid() << " reading process " << pid << endl;
    subscribe_process(priority, pid);
  }
  else if (a == SIGUSR2){
    read(pd[0], &pid, sizeof(int));
    cout << getpid() << " reading finished process " << pid << endl;
    remove_priority(pid);
  }
}


int main(int argc, char** argv) {
  // Creating pipes
  if (pipe(pd) < 0)
    exit(1);

  cout << getpid() << " created pipes and starting... " << endl;

  pid_t scheduler_pid = getpid();
  // Create proccess to run the rpc calls
  pid_t C1 = fork();
  if (C1 == 0){
    run_processes(scheduler_pid);
  }
  else{
    // Define handlers
    struct sigaction action;
    action.sa_handler = &read_from_pipe;
    action.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);

    schedule();
  }
}
