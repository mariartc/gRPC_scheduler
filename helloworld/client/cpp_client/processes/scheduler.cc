#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

#define TCP_PORT    35001
#define TCP_BACKLOG 5

char addrstr[INET_ADDRSTRLEN];
char buf[100];

vector<int> pids, priorities;
int client, sd, debug = false;
int max_pid = -1, running_pid = -1, max_priority = -1, sigusr1 = 0;
struct sockaddr_in sa;
socklen_t len;
sigset_t block_mask_process;


int get_maximum_priority(){
    if (priorities.size() == 0){
        max_priority = -1;
        return -1;
    }

    int max_i = -1, max = 0, n = priorities.size();
    for(int i = 0; i < n; i++){
        if (priorities[i] > max){
            max_i = i;
            max = priorities[i];
        }
    }
    max_priority = priorities[max_i];
    return pids[max_i];
}


void subscribe_process(int priority, int pid){
    priorities.push_back(priority);
    pids.push_back(pid);
    if (max_pid == -1 or priority > max_priority){
        max_pid = pid;
        max_priority = priority;
        if (debug)
            cout << "subscribe_priority: new max_pid=" << max_pid 
                << ", new priority=" << max_priority << endl;
    }
}


void remove_priority(int pid){
    int n = pids.size();
    for(int i = 0; i < n; i++){
        if (pids[i] == pid){
            pids.erase(pids.begin() + i);
            priorities.erase(priorities.begin() + i);
            break;
        }
    }
    max_pid = get_maximum_priority();
    if (debug)
        cout << "remove_priority: new max_pid=" << max_pid 
            << ", new priority=" << max_priority << endl;
}


bool is_subscribe(){
    char subscribe[10] = "subscribe";
    int k = memcmp(buf, subscribe, sizeof(subscribe)-1);
    return k == 0;
}


bool is_remove(){
    char remove[7] = "remove";
    int k = memcmp(buf, remove, sizeof(remove)-1);
    return k == 0;
}


void parse_buffer(ssize_t n){
    if(n > 7){
        if (is_subscribe()){
            int i = 0, j = 0;
            char temp[10];
            if (debug)
                cout << getpid() << " read " << buf << "\n";
            while(buf[9 + i] != ':'){
                memcpy(temp + i, buf + 9 + i, sizeof(char));
                i++;
            }
            char pid[i];
            memcpy(pid, temp, sizeof(pid));
            while(buf[10 + i + j] != '\0'){
                memcpy(temp + j, buf + i + j + 10, sizeof(char));
                j++;
            }
            char priority[j];
            memcpy(priority, temp, sizeof(priority));
            int int_pid = atoi(pid), int_priority = atoi(priority);
            if(debug)
                printf("%d subscribing %d with priority %d\n", getpid(), int_pid, int_priority);
            subscribe_process(int_priority, int_pid);
        }
        else if(is_remove()){
            if (debug)
                cout << getpid() << " read " << buf << "\n";
            int i = 0;
            char temp[10];
            while(buf[6 + i] != '\0'){
                memcpy(temp + i, buf + 6 + i, sizeof(char));
                i++;
            }
            char pid[i];
            memcpy(pid, temp, sizeof(pid));
            int int_pid = atoi(pid);
            if(debug)
                printf("%d removing %d\n", getpid(), int_pid);
            remove_priority(int_pid);
        }
        else{
            printf("%d read something wrong: %s\n", getpid(), buf);
        }
    }
}


// SIGUSR1 handler
void read_from_socket(int a){
    if (a == -1) sigprocmask(SIG_BLOCK, &block_mask_process, NULL);
    ssize_t n;
    n = read(client, buf, sizeof(buf));
    if (n > 0) parse_buffer(n);
    sigusr1 = 0;
    if (a == -1) sigprocmask(SIG_UNBLOCK, &block_mask_process, NULL);
}


void schedule(){
    while(1){
        // Every 10000000 iterations read from socket
        // in case a signal SIGUSR1 was not delivered
        if (sigusr1 >= 10000000) read_from_socket(-1);
        if (max_pid != -1 and max_pid != running_pid){
            sigprocmask(SIG_BLOCK, &block_mask_process, NULL);
            if (running_pid != -1){
                kill(running_pid, SIGSTOP);
                if (debug)
                    cout << getpid() << " sent " 
                    << running_pid << " to sleep" << endl;
            }
            running_pid = max_pid;
            kill(max_pid, SIGCONT);
            if (debug)
                cout << getpid() << " waking up " << max_pid << endl;
            sigprocmask(SIG_UNBLOCK, &block_mask_process, NULL);
        }
        sigusr1 += 1;
    }
}


bool create_sockets(){
    // Create TCP/IP socket
    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return false;
    }
    cout << getpid() << " created TCP socket\n";

    /* Bind to a well-known port */
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind");
        return false;
	}
    cout << getpid() << " bound TCP socket to port " << TCP_PORT << endl;

	/* Listen for incoming connections */
	if (listen(sd, TCP_BACKLOG) < 0) {
		perror("listen");
        return false;
	}

    len = sizeof(struct sockaddr_in);
    if ((client = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
        perror("accept");
        return false;
    }

    if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
        perror("could not format IP address");
        return false;
    }

    cout << getpid() << " got incoming connection from " << addrstr 
        << ":" << ntohs(sa.sin_port) << endl;

    return true;
}


void close_connection(int a){
    close(client);
    exit(0);
}


int main(int argc, char** argv) {

    if (argc == 2 && argv[1] == "--debug")
        debug = true;

    if(not create_sockets())
        exit(1);

    sigemptyset(&block_mask_process);
    sigaddset(&block_mask_process, SIGUSR1);

    // Define handlers
    struct sigaction action_sigusr1;
    sigset_t block_mask_sigusr1;

    sigemptyset(&block_mask_sigusr1);
    sigaddset(&block_mask_sigusr1, SIGUSR1);
    action_sigusr1.sa_mask = block_mask_sigusr1;
    action_sigusr1.sa_handler = &read_from_socket;
    action_sigusr1.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &action_sigusr1, NULL);

    struct sigaction action_sigint;

    action_sigint.sa_handler = &close_connection;
    action_sigint.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action_sigint, NULL);

    schedule();
}
