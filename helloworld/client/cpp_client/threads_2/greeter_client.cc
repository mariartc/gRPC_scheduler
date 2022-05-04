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
thread::id default_id = get_id();
thread::id max_id = default_id, running_id = default_id;
int max_priority = -1, n;
vector<tuple<thread::id, int>> priorities;
unordered_map<thread::id, bool> sleep_map;
vector<std::thread> threads;
ofstream output_file;
mutex output_file_mutex, priorities_mutex, cout_mutex, sleep_mutex, subscribe_mutex, unsubscribe_mutex;
bool stop_scheduler = false, use_scheduler = false, debug = false;

queue<tuple<thread::id, int>> subscribe;
queue<thread::id> unsubscribe;


bool should_sleep(thread::id id){
  sleep_mutex.lock();
  bool temp = sleep_map[id] == 0;
  sleep_mutex.unlock();
  return temp;
}


void sleep_thread(thread::id id){
  while(should_sleep(id));
}


class GreeterClient {
  public:
    GreeterClient(shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

    FloatNumber ComputeMean(int *arr, int arr_length, thread::id id) {
      ClientContext context;
      FloatNumber reply;
      
      std::unique_ptr<ClientWriter<IntNumber> > writer(
          stub_->ComputeMean(&context, &reply));
      if(debug){
        cout_mutex.lock();
        cout << id << " starts sending array" << endl;
        cout_mutex.unlock();
      }

      IntNumber int_num_arr[arr_length];
      for (int i = 0; i < arr_length; i++){
        IntNumber int_num;
        int_num.set_value(arr[i]);
        int_num_arr[i] = int_num;
      }

      for (int i = 0; i < arr_length; i++){
        if(use_scheduler) sleep_thread(id);
        if (!writer->Write(int_num_arr[i])){
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

  private:
    unique_ptr<Greeter::Stub> stub_;
};


thread::id get_maximum_priority(){
  if (priorities.size() == 0){
      max_priority = -1;
      return default_id;
  }
  int max_i = -1, max = 0, n = priorities.size();
  for(int i = 0; i < n; i++){
      if (get<1>(priorities[i]) > max){
          max_i = i;
          max = get<1>(priorities[i]);
      }
  }
  max_priority = get<1>(priorities[max_i]);
  thread::id returned_id = get<0>(priorities[max_i]);
  return returned_id;
}


void subscribe_thread(){
  subscribe_mutex.lock();
  tuple<thread::id, int> front_element = subscribe.front();
  subscribe.pop();
  subscribe_mutex.unlock();

  thread::id id = get<0>(front_element);
  int priority = get<1>(front_element);

  priorities.push_back(front_element);

  sleep_mutex.lock();
  sleep_map[id] = 0;
  sleep_mutex.unlock();

  if (max_id == default_id or priority > max_priority){
    max_id = id;
    max_priority = priority;
    if(debug){
      cout_mutex.lock();
      cout << "subscribe_priority: new max_id=" << max_id 
          << ", new priority=" << max_priority << endl;
      cout_mutex.unlock();
    }
  }
}


void unsubscribe_thread(){
  unsubscribe_mutex.lock();
  thread::id id = unsubscribe.front();
  unsubscribe.pop();
  unsubscribe_mutex.unlock();

  for(int i = 0; i < priorities.size(); i++){
    if (get<0>(priorities[i]) == id){
      priorities.erase(priorities.begin() + i);
      break;
    }
  }

  sleep_mutex.lock();
  sleep_map.erase(id);
  sleep_mutex.unlock();

  max_id = get_maximum_priority();
  if(running_id == id) running_id = default_id;

  if(debug){
    cout_mutex.lock();
    cout << priorities.size() << " remove_priority: new max_id=" << max_id << ", new priority=" << max_priority << endl;
    cout_mutex.unlock();
  }
}


vector<string> parse_line(string line, int &priority){
  vector<string> words{};
  vector<string> numbers{};
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
    numbers.push_back(line.substr(0, pos));
    line.erase(0, pos + 1);
  }
  numbers.push_back(last_number);

  priority = atoi(words[2].c_str());

  return numbers;
}


void run_rpc(string line) {
  int priority;
  vector<string> numbers = parse_line(line, priority);

  int arr_length = numbers.size();
  int arr[arr_length];
  for(int i = 0; i < numbers.size(); i++)
    arr[i] = atoi(numbers[i].c_str());

  auto start = high_resolution_clock::now();
  thread::id id = get_id();

  if(use_scheduler){
    subscribe_mutex.lock();
    subscribe.push(make_tuple(id, priority));
    subscribe_mutex.unlock();
  }

  if(debug){
    cout_mutex.lock();
    cout << id << " going to sleep..." << endl;
    cout_mutex.unlock();
  }

  if(use_scheduler){
    sleep_mutex.lock();
    sleep_map[id] = 0;
    sleep_mutex.unlock();
    sleep_thread(id);
  }

  GreeterClient greeter(
    grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

  if(use_scheduler) sleep_thread(id);

  if(debug){
    cout_mutex.lock();
    cout << id << " woke up!" << endl;
    cout_mutex.unlock();
  }
  
  FloatNumber mean = greeter.ComputeMean(arr, arr_length, id);
  auto stop = high_resolution_clock::now();

  if(debug){
    cout_mutex.lock();
    cout << id << " got mean: " << mean.value() << endl;
    cout_mutex.unlock();
  }
  
  auto duration = duration_cast<microseconds>(stop - start);
  output_file_mutex.lock();
  output_file << line + " " << duration.count() << endl;
  output_file_mutex.unlock();
  if(use_scheduler){
    subscribe_mutex.lock();
    unsubscribe.push(id);
    subscribe_mutex.unlock();
  }
}


void send_to_sleep(thread::id id){
  sleep_mutex.lock();
  sleep_map[id] = 0;
  sleep_mutex.unlock();
}


void wake_up(thread::id id){
  sleep_mutex.lock();
  sleep_map[id] = 1;
  sleep_mutex.unlock();
}


void scheduler(){
  while(not stop_scheduler){
    unsubscribe_mutex.lock();
    n = unsubscribe.size();
    unsubscribe_mutex.unlock();
    if(n > 0) unsubscribe_thread();
    subscribe_mutex.lock();
    n = subscribe.size();
    subscribe_mutex.unlock();
    if(n > 0) subscribe_thread();

    if (max_id != default_id and max_id != running_id){
        if (running_id != default_id){
            send_to_sleep(running_id);
          if(debug){
            cout_mutex.lock();
            cout << get_id() << " sent " << running_id << " to sleep" << endl;
            cout_mutex.unlock();
          }
        }
        wake_up(max_id);
        running_id = max_id;
        if(debug){
          cout_mutex.lock();
          cout << get_id() << " waking up " << max_id << endl;
          cout_mutex.unlock();
        }
    }
  }
}


int main(int argc, char** argv) {

  if(argc < 3){
		cout << "Usage: ./greeter_client <input_file> <output_file> [--scheduler] [--debug]\n";
		exit(0);
	}
  
  if(argc == 4 && !strcmp(argv[3], "--scheduler") || argc == 5 && !strcmp(argv[3], "--scheduler")) use_scheduler = true;

  if(argc == 4 && !strcmp(argv[3], "--debug") || argc == 5 && !strcmp(argv[4], "--debug")) debug = true;

  auto start_total = high_resolution_clock::now();
  std::thread sch;

  string script_name = argv[1], output = argv[2];
  string line;
  fstream input_file(script_name, ios::in);
  output_file.open(output);
  
  if (use_scheduler) sch = std::thread(scheduler);

  if(input_file.is_open() && output_file.is_open()){
    start_total = high_resolution_clock::now();
    while (getline(input_file,line)){
      threads.push_back(std::thread(run_rpc, line));
    }
    input_file.close();
  }
  else cout << "Unable to open file";
  
  for (int i = 0; i < threads.size(); i++) {
    threads[i].join();
  }

  if(use_scheduler){
    stop_scheduler = true;
    sch.join();
  }

  auto stop_total = high_resolution_clock::now();
  auto duration_total = duration_cast<microseconds>(stop_total - start_total);

  output_file << "Total_time: " << duration_total.count();
  output_file.close();
}
