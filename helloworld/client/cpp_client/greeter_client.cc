#include <algorithm>
#include <chrono>
#include <fstream>
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
using namespace std::chrono;

#define TCP_PORT    35001
#define HOSTNAME    "localhost"

int sd, scheduler_pid;
char buf[100];
struct hostent *hp;
struct sockaddr_in sa;
ofstream output_file ("../../execution_times.txt");

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

      IntNumber int_num_arr[arr_length];
      for (int i = 0; i < arr_length; i++){
        IntNumber int_num;
        int_num.set_value(arr[i]);
        int_num_arr[i] = int_num;
      }

      for (int i = 0; i < arr_length; i++){
        if (!writer->Write(int_num_arr[i])) {
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


void run_rpc(int r, string long_str, int *arr, int arr_length, string descr) {
  GreeterClient greeter(
    grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
  cout << getpid() << " going to sleep..." << endl;
  // Wait for scheduler to notify
  raise(SIGSTOP);
  cout << getpid() << " woke up! " << endl;
  auto start = high_resolution_clock::now();
  if (r == 0){
      LongString lng_str = greeter.SendLongString(long_str);
      auto stop = high_resolution_clock::now();
      auto duration = duration_cast<microseconds>(stop - start);
      output_file << descr + " " << duration.count() << endl;
      cout << getpid() << " got long string: " << lng_str.str() << endl;
      write_to_socket("remove" + to_string(getpid()));
      kill(scheduler_pid, SIGUSR1);
      raise(SIGTERM);
  }
  else if (r == 1){
    FloatNumber mean = greeter.ComputeMean(arr, arr_length);
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);
    output_file << descr + " " << duration.count() << endl;
    cout << getpid() << " got mean: " << mean.value() << endl;
    write_to_socket("remove" + to_string(getpid()));
    kill(scheduler_pid, SIGUSR1);
    raise(SIGTERM);
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


void parse_line(string line){
  vector<string> words{};
  string word, original_line = line;
  pid_t C1;

  size_t pos = 0;

  int last_space = line.find_last_of(" ");
  string last_word = line.substr(last_space + 1, line.size() - 1 - last_space);
  while ((pos = line.find(" ")) != string::npos){
    word = line.substr(0, pos);
    words.push_back(line.substr(0, pos));
    line.erase(0, pos + 1);
  }
  words.push_back(last_word);

  if (words[0] == "compute_mean"){
    vector<string> numbers{};
    pos = 0;
    line = words[1];
    int last_comma = line.find_last_of(",");
    string last_number = line.substr(last_comma + 1, line.size() - 1 - last_comma);
    while ((pos = line.find(",")) != string::npos){
      numbers.push_back(line.substr(0, pos));
      line.erase(0, pos + 1);
    }
    numbers.push_back(last_number);

    int arr[numbers.size()];
    for(int i = 0; i < numbers.size(); i++)
      arr[i] = atoi(numbers[i].c_str());

    C1 = fork();
    if (C1 == 0){
      run_rpc(1, "", arr, sizeof(arr)/sizeof(arr[0]), original_line);
    }
    else {
      // Wait fot C1 to fall asleep
      waitpid(C1, NULL, WUNTRACED);
      // Subscribe C1 to scheduler
      write_to_socket("subscribe" + to_string(C1) + ":" + words[2]);
      // Send signal to scheduler to read the pipe
      kill(scheduler_pid, SIGUSR1);
    }
  }
  else if (words[0] == "send_long_string"){
    // Wait a little so that the first rpc starts sending
    // in order to observe the switching between processes
    // this_thread::sleep_for(std::chrono::milliseconds(8));
    C1 = fork();
    if (C1 == 0){
      run_rpc(0, words[1], NULL, -1, original_line);
    }
    else{
      // Wait fot C1 to fall asleep
      waitpid(C1, NULL, WUNTRACED);
      // Subscribe C2 to scheduler
      write_to_socket("subscribe" + to_string(C1) + ":" + words[2]);
      // Send signal to scheduler to read the pipe
      kill(scheduler_pid, SIGUSR1);
    }
  }
  else
    cout << getpid() << " read wrong input" << endl;
}


int main(int argc, char** argv) {
  if(argc < 3 || atoi(argv[1]) <= 0){
		cout << "Usage: ./greeter_client <scheduler_pid> <script_name>\n";
		exit(0);
	}

  if(not connect_to_socket())
    exit(1);

  scheduler_pid = atoi(argv[1]);
  string script_name = argv[2];
  int processes = 0;
  string line;
  fstream input_file(script_name, ios::in);
  if (input_file.is_open() && output_file.is_open()){
    while (getline(input_file,line)){
      processes += 1;
      parse_line(line);
    }
    input_file.close();
  }
  else cout << "Unable to open file";

  for(int i = 0; i < processes; i++) wait(NULL);
  output_file.close();
}
