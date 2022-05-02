#include <algorithm>
#include <chrono>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <mutex>
#include <netdb.h>
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
int max_priority = -1;
vector<tuple<thread::id, int, int>> ids;
vector<std::thread> threads;
ofstream output_file;
mutex output_file_mutex, ids_mutex, cout_mutex;
bool stop_scheduler = false, use_scheduler = false, debug = false;


bool should_sleep(thread::id id){
  ids_mutex.lock();
  int n = ids.size();
  ids_mutex.unlock();
  for (int i = 0; i < n; i++){
    if (get<0>(ids[i]) == id){
      bool temp = get<1>(ids[i]) == 1;
      return temp;
    }
  }
  return true;
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
  if (ids.size() == 0){
      max_priority = -1;
      return default_id;
  }
  int max_i = -1, max = 0, n = ids.size();
  for(int i = 0; i < n; i++){
      if (get<2>(ids[i]) > max){
          max_i = i;
          max = get<2>(ids[i]);
      }
  }
  max_priority = get<2>(ids[max_i]);
  thread::id returned_id = get<0>(ids[max_i]);
  return returned_id;
}


void subscribe_thread(thread::id id, int priority){
  ids_mutex.lock();
  ids.push_back(make_tuple(id, 1, priority));
  if (max_id == default_id or priority > max_priority){
    max_id = id;
    max_priority = priority;
    ids_mutex.unlock();
    if(debug){
      cout_mutex.lock();
      cout << "subscribe_priority: new max_id=" << max_id 
          << ", new priority=" << max_priority << endl;
      cout_mutex.unlock();
    }
  }
  else
    ids_mutex.unlock();
}


void unsubscribe_thread(thread::id id){
  ids_mutex.lock();
  int n = ids.size();
  for(int i = 0; i < n; i++){
    if (get<0>(ids[i]) == id){
      ids.erase(ids.begin() + i);
      break;
    }
  }
  max_id = get_maximum_priority();
  ids_mutex.unlock();
  if(debug){
    cout_mutex.lock();
    cout << "remove_priority: new max_id=" << max_id << ", new priority=" << max_priority << endl;
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

  if(use_scheduler) subscribe_thread(id, priority);
  GreeterClient greeter(
    grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

  if(debug){
    cout_mutex.lock();
    cout << id << " going to sleep..." << endl;
    cout_mutex.unlock();
  }

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
  if(use_scheduler) unsubscribe_thread(id);
}


void send_to_sleep(thread::id id){
  ids_mutex.lock();
  for (int i = 0; i < ids.size(); i++){
    if (get<0>(ids[i]) == id)
      get<1>(ids[i]) = 1;
  }
  ids_mutex.unlock();
}


void wake_up(thread::id id){
  ids_mutex.lock();
  for (int i = 0; i < ids.size(); i++){
    if (get<0>(ids[i]) == id)
      get<1>(ids[i]) = 0;
  }
  ids_mutex.unlock();
}


void scheduler(){
  while(not stop_scheduler){
      if (max_id != default_id and max_id != running_id){
          if (running_id != default_id){
              send_to_sleep(running_id);
            if(debug){
              cout_mutex.lock();
              cout << get_id() << " sent " << running_id << " to sleep" << endl;
              cout_mutex.unlock();
            }
          }
          running_id = max_id;
          wake_up(max_id);
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

  if(argc == 4 && !strcmp(argv[3], "--debug") || argc == 5 && !strcmp(argv[3], "--debug")) debug = true;

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
