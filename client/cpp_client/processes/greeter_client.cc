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

int sd, scheduler_pid, debug = false, scheduler = false;
char buf[100];
struct hostent *hp;
struct sockaddr_in sa;
ofstream output_file;

class GreeterClient {
  public:
    GreeterClient(shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

    FloatNumber ComputeMean(int *arr, int arr_length) {
      ClientContext context;
      FloatNumber reply;
      
      std::unique_ptr<ClientWriter<IntNumber> > writer(
          stub_->ComputeMean(&context, &reply));
      if(debug)
        cout << getpid() << " starts sending array" << endl;

      IntNumber int_num_arr[arr_length];
      for (int i = 0; i < arr_length; i++){
        IntNumber int_num;
        int_num.set_value(arr[i]);
        int_num_arr[i] = int_num;
      }

      for (int i = 0; i < arr_length; i++){
        if(!writer->Write(int_num_arr[i])) {
          cout << "Broken stream" << endl;
          break;
        }
      }

      writer->WritesDone();
      Status status = writer->Finish();

      if(status.ok()) {
        return reply;
      } else {
        FloatNumber error;
        error.set_value(-1);
        return error;
      }
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

  if(debug)
    cout << getpid() << " wrote "
    << buf << " to socket" << endl;
}


void run_rpc(int *arr, int arr_length, string descr) {
  auto start = high_resolution_clock::now();
  GreeterClient greeter(
    grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

  if(scheduler){
    if(debug) cout << getpid() << " going to sleep..." << endl;
    // Wait for scheduler to notify
    raise(SIGSTOP);
    if(debug) cout << getpid() << " woke up! " << endl;
  }

  FloatNumber mean = greeter.ComputeMean(arr, arr_length);
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<microseconds>(stop - start);
  output_file << descr + " " << duration.count() << endl;
  if(debug) cout << getpid() << " got mean: " << mean.value() << endl;

  if(scheduler){
    write_to_socket("remove" + to_string(getpid()));
    kill(scheduler_pid, SIGUSR1);
  }
  raise(SIGTERM);
}


bool connect_to_socket(){
  // Create TCP/IP socket
  if((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return false;
	}
  cout << getpid() << " created TCP socket\n";

	// Look up remote hostname on DNS
	if(!(hp = gethostbyname(HOSTNAME))) {
    cout << getpid() << " failed DNS lookup for host " << HOSTNAME << endl;
		return false;
	}

	/* Connect to remote TCP port */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
  cout << getpid() << " connecting to remote host..." << endl;
	if(connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("connect");
		return false;
	}
  cout << getpid() << " connected!" << endl;
  return true;
}


void close_connection(int a){
  close(sd);
  exit(0);
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
  if(C1 == 0){
    run_rpc(arr, sizeof(arr)/sizeof(arr[0]), original_line);
  }
  else {
    if(scheduler){
      // Wait fot C1 to fall asleep
      waitpid(C1, NULL, WUNTRACED);
      // Subscribe C1 to scheduler
      write_to_socket("subscribe" + to_string(C1) + ":" + words[2]);
      // Send signal to scheduler to read the pipe
      kill(scheduler_pid, SIGUSR1);
    }
  }
}


int main(int argc, char** argv) {
  // Define handler for SIGINT
  struct sigaction action;
  action.sa_handler = &close_connection;
  action.sa_flags = SA_RESTART;
  sigaction(SIGINT, &action, NULL);

  if(argc < 3){
		cout << "Usage: ./greeter_client <input_file> <output_file> [scheduler_pid] [--debug]\n";
		exit(0);
	}

  if((argc == 4 && argv[3] == "--debug") || (argc == 5 && argv[4] == "--debug"))
    debug = true;

  if(argc >= 4 && (scheduler_pid = atoi(argv[3])) > 0)
    scheduler = true;

  if(scheduler)
    if(not connect_to_socket())
      exit(1);

  auto start_total = high_resolution_clock::now();

  int wpid, status = 0;
  string script_name = argv[1], output = argv[2];
  string line;
  fstream input_file(script_name, ios::in);
  output_file.open(output);

  if(input_file.is_open() && output_file.is_open()){
    start_total = high_resolution_clock::now();
    while (getline(input_file,line)){
      parse_line(line);
    }
    input_file.close();
  }
  else cout << "Unable to open file";

  // Wait for all children to finish
  while ((wpid = wait(&status)) > 0);

  auto stop_total = high_resolution_clock::now();
  auto duration_total = duration_cast<microseconds>(stop_total - start_total);

  output_file << "Total_time: " << duration_total.count();
  output_file.close();
  if(scheduler) close(sd);
}
