#include <algorithm>
#include <chrono>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <mutex>
#include <netdb.h>
#include <queue>
#include <string>
#include <string.h>
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
using namespace std::this_thread;

#define TCP_PORT    35001
#define HOSTNAME    "localhost"

int max_id = -1, max_priority = -1;
unordered_map<int, int> priorities;
unordered_map<int, bool> sleep_map;
unordered_map<int, std::thread> threads;
ofstream output_file;
ifstream input_file;
mutex output_file_mutex, cout_mutex, sleep_mutex, unsubscribe_mutex;
bool use_scheduler = false, debug = false;

queue<int> unsubscribe;


bool should_sleep(int id){
  sleep_mutex.lock();
  bool temp = sleep_map[id] == 0;
  sleep_mutex.unlock();
  return temp;
}


void sleep_thread(int id){
  while(should_sleep(id));
}


void print_sleep_map(){
  cout_mutex.lock();
  for (auto element : sleep_map){
    cout << element.first << ": " << element.second << endl;
  }
  cout_mutex.unlock();
}


class GreeterClient {
  public:
    GreeterClient(shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

    FloatNumber ComputeMean(vector<int> arr, int id) {
      int arr_length = arr.size();
      ClientContext context;
      FloatNumber reply;

      if(use_scheduler) sleep_thread(id);
      
      std::unique_ptr<ClientWriter<IntNumber> > writer(
          stub_->ComputeMean(&context, &reply));

      IntNumber int_num_arr[arr_length];
      for (int i = 0; i < arr_length; i++){
        IntNumber int_num;
        int_num.set_value(arr[i]);
        int_num_arr[i] = int_num;
      }

      for (int i = 0; i < arr_length; i++){
        if (!writer->Write(int_num_arr[i])){
          cout << "Broken stream" << endl;
          break;
        }
        if(use_scheduler && (i < arr_length - 1)) sleep_thread(id);
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

  private:
    unique_ptr<Greeter::Stub> stub_;
};


int get_maximum_priority(){
  if (priorities.size() == 0){
      max_priority = -1;
      return -1;
  }
  int max_id = -1, max_priority = 0;
  for (auto element : priorities){
    if (element.second > max_priority || element.second == max_priority && element.first < max_id) {
      max_id = element.first;
      max_priority = element.second;
    }
  }

  return max_id;
}


vector<int> parse_line(string line, int &priority){
  vector<string> words{};
  vector<int> numbers{};
  string word;

  size_t pos = 0;

  int last_space = line.find_last_of(" ");
  string last_word = line.substr(last_space + 1, line.size() - 1 - last_space);
  while ((pos = line.find(" ")) != string::npos){
    word = line.substr(0, pos);
    words.push_back(line.substr(0, pos));
    line.erase(0, pos + 1);
  }
  words.push_back(last_word);

  pos = 0;
  line = words[1];
  int last_comma = line.find_last_of(",");
  string last_number = line.substr(last_comma + 1, line.size() - 1 - last_comma);
  while ((pos = line.find(",")) != string::npos){
    numbers.push_back(atoi((line.substr(0, pos)).c_str()));
    line.erase(0, pos + 1);
  }
  numbers.push_back(atoi(last_number.c_str()));

  priority = atoi(words[2].c_str());

  return numbers;
}


void run_rpc(string line, vector<int> arr, int id, bool active) {
  auto start = high_resolution_clock::now();

  if(debug && use_scheduler){
    cout_mutex.lock();
    cout << id << " going to sleep..." << endl;
    cout_mutex.unlock();
  }

  if(use_scheduler && !active){
    sleep_thread(id);
  }

  GreeterClient greeter(
    grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

  if(debug && use_scheduler){
    cout_mutex.lock();
    cout << id << " woke up!" << endl;
    cout_mutex.unlock();
  }

  
  FloatNumber mean = greeter.ComputeMean(arr, id);
  auto stop = high_resolution_clock::now();

  if(debug && use_scheduler){
    cout_mutex.lock();
    cout << id << " got mean: " << mean.value() << endl;
    cout_mutex.unlock();
  }
  
  unsubscribe_mutex.lock();
  unsubscribe.push(id);
  unsubscribe_mutex.unlock();

  auto duration = duration_cast<microseconds>(stop - start);
  output_file_mutex.lock();
  output_file << line + " " << duration.count() << endl;
  output_file_mutex.unlock();
}


void change_sleep_map(int id, bool value){
  sleep_mutex.lock();
  sleep_map[id] = value;
  sleep_mutex.unlock();
}


void subscribe_thread(string line, int id){
  int priority;
  vector<int> numbers;

  numbers = parse_line(line, priority);
  priorities[id] = priority;
  if(!use_scheduler) {
    threads[id] = std::thread(run_rpc, line, numbers, id, true);
    return;
  }
  if (priority > max_priority){
    if(max_id != -1){
      change_sleep_map(max_id, 0);
    }
    max_id = id;
    max_priority = priority;
    change_sleep_map(id, 1);
    threads[id] = std::thread(run_rpc, line, numbers, id, true);

    if(debug){
      cout_mutex.lock();
      cout << "Subscribe: new max_id=" << max_id
          << ", new priority=" << max_priority << endl;
      cout_mutex.unlock();
    }
  }
  else {
    threads[id] = std::thread(run_rpc, line, numbers, id, false);
    if(debug){
      cout_mutex.lock();
      cout << "Subscribe: " << id << endl;
      cout_mutex.unlock();
    }
  }
}


void unsubscribe_thread() {
    unsubscribe_mutex.lock();
    if (unsubscribe.size() > 0) {
      int unsubscribe_id = unsubscribe.front();
      unsubscribe.pop();
      unsubscribe_mutex.unlock();
      threads[unsubscribe_id].join();
      priorities.erase(unsubscribe_id);
      if(!use_scheduler) return;
      max_id = get_maximum_priority();
      if (max_id == -1) return;
      change_sleep_map(max_id, 1);
      max_priority = priorities[max_id];
      if(debug){
        cout_mutex.lock();
        cout << "Unsubscribe: " << unsubscribe_id << ", new max_id=" << max_id 
              << ", new max_priority=" << max_priority << endl;
        cout_mutex.unlock();
      }
    }
    else unsubscribe_mutex.unlock();
}


void scheduler(int lines){
  auto start_total = high_resolution_clock::now();
  string line;
  int priority;
  vector<int> numbers;
  int id = 1, unsubscribe_id = -1;
  std::thread modify_sleep_thread;

  while (getline(input_file,line)){
    subscribe_thread(line, id);
    unsubscribe_thread();
    id += 1;
  }
  while(priorities.size() > 0){
    unsubscribe_thread();
  }

  auto stop_total = high_resolution_clock::now();
  auto duration_total = duration_cast<microseconds>(stop_total - start_total);

  output_file << "Total_time: " << duration_total.count();
}


int main(int argc, char** argv) {

  if(argc < 3){
		cout << "Usage: ./greeter_client <input_file> <output_file> [--scheduler] [--debug]\n";
		exit(0);
	}
  
  if(argc == 4 && !strcmp(argv[3], "--scheduler") || argc == 5 && !strcmp(argv[3], "--scheduler")) use_scheduler = true;

  if(argc == 4 && !strcmp(argv[3], "--debug") || argc == 5 && !strcmp(argv[4], "--debug")) debug = true;

  std::thread sch;

  string script_name = argv[1], output = argv[2];
  string line;
  input_file.open(script_name);
  output_file.open(output);
  int lines;

  if(input_file.is_open()){
    while (getline(input_file,line)){
      lines += 1;
    }
    input_file.close();
  }
  else cout << "Unable to open file";
  
  input_file.open(script_name);
  if(input_file.is_open() && output_file.is_open()){
    sch = std::thread(scheduler, lines);
  }
  else cout << "Unable to open file";

  sch.join();
  output_file.close();
}
