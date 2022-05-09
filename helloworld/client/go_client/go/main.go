/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// Package main implements a client for Greeter service.
package main

import (
	"bufio"
	"context"
	"flag"
	"log"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"google.golang.org/grpc"
	pb "google.golang.org/grpc/examples/helloworld/helloworld"
)

var (
	addr                   = flag.String("addr", "localhost:50051", "the address to connect to")
	debug                  = false
	input_file             = ""
	output_file            = ""
	priorities_and_threads = make(map[int]map[int]Thread)
	// priorities_and_threads = make(map[int][]Thread)
	threads       = make(map[int]Thread)
	use_scheduler = false
	mutex         sync.Mutex
)

type Thread struct {
	priority int
	channel  chan bool
}

type SubscribeElement struct {
	priority int
	numbers  []int32
	descr    string
}

func write_to_file(time int64, descr string) {
	mutex.Lock()
	f, err := os.OpenFile(output_file, os.O_APPEND|os.O_WRONLY|os.O_CREATE, 0600)
	if err != nil {
		panic(err)
	}

	defer f.Close()
	time_int := int(time)
	if int64(time_int) != time {
		log.Printf("Error converting int64 to int\n")
	}
	time_str := strconv.Itoa(time_int)
	text := descr + " " + time_str + "\n"

	if _, err = f.WriteString(text); err != nil {
		panic(err)
	}
	mutex.Unlock()
}

func thread(state <-chan bool, unsubscribe chan<- int, id int, numbers []int32, wg *sync.WaitGroup, descr string, active bool) {
	if debug {
		log.Printf("Thread %v going to sleep...", id)
	}

	conn, err := grpc.Dial(*addr, grpc.WithInsecure())
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	client := pb.NewGreeterClient(conn)

	var numbersInt []*pb.IntNumber
	for i := 0; i < len(numbers); i++ {
		numbersInt = append(numbersInt, &pb.IntNumber{Value: numbers[i]})
	}
	start := time.Now()

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	stream, err := client.ComputeMean(ctx)
	if err != nil {
		log.Fatalf("%v.ComputeMean(_) = _, %v", client, err)
	}

	time.Sleep(7000 * time.Microsecond)
	for _, number := range numbersInt {

		if active && (len(state) != 0) {
			cmd := <-state
			switch cmd {
			case false:
				active = false
				if debug {
					log.Printf("Thread %v going to sleep...", id)
				}
			case true:
				active = true
				if debug {
					log.Printf("Thread %v woke up!", id)
				}
			}
		}
		if !active {
			select {
			case cmd := <-state:
				switch cmd {
				case false:
					active = false
				case true:
					active = true
					if debug {
						log.Printf("Thread %v woke up!", id)
					}
				}
			}
		}

		if err := stream.Send(number); err != nil {
			log.Fatalf("Thread %v %v.Send(%v) = %v", id, stream, number, err)
		}
	}
	reply, err := stream.CloseAndRecv()
	if err != nil {
		log.Fatalf("Thread %v %v.ComputeMean() got error %v, want %v", id, stream, err, nil)
	}
	elapsed := time.Since(start)
	if use_scheduler {
		unsubscribe <- id
	}
	microseconds_elapsed := elapsed.Microseconds()
	go write_to_file(microseconds_elapsed, descr)
	if debug {
		log.Printf("Thread %v Got mean: %v", id, reply.Value)
	}
	wg.Done()
}

func get_max_priority() int {
	max_priority := 0
	for priority, _ := range priorities_and_threads {
		if priority > max_priority {
			max_priority = priority
		}
	}

	return max_priority
}

func set_channels(priority int, what bool) {
	for _, thread := range priorities_and_threads[priority] {
		thread.channel <- what
	}
}

func scheduler(subscribe <-chan SubscribeElement, wg *sync.WaitGroup) {
	unsubscribe := make(chan int, 100)
	id := 1
	max_priority := 0

	for {
		select {
		case subscribe_elem := <-subscribe:
			state_channel := make(chan bool, 2)
			priority := subscribe_elem.priority
			numbers := subscribe_elem.numbers
			descr := subscribe_elem.descr
			threads[id] = Thread{priority: subscribe_elem.priority}

			if use_scheduler {
				if _, ok := priorities_and_threads[priority]; ok {
					priorities_and_threads[priority][id] = Thread{priority: priority, channel: state_channel}
				} else {
					priorities_and_threads[priority] = make(map[int]Thread)
					priorities_and_threads[priority][id] = Thread{priority: priority, channel: state_channel}
				}
				priorities_and_threads[priority][id] = Thread{priority: priority, channel: state_channel}
				if max_priority == 0 {
					max_priority = priority
					go thread(state_channel, unsubscribe, id, numbers, wg, descr, true)
				} else if (priority == max_priority) || (max_priority == 0) {
					go thread(state_channel, unsubscribe, id, numbers, wg, descr, true)
				} else if priority > max_priority {
					set_channels(max_priority, false)
					go thread(state_channel, unsubscribe, id, numbers, wg, descr, true)
					max_priority = priority
				} else {
					go thread(state_channel, unsubscribe, id, numbers, wg, descr, false)
				}
			} else {
				go thread(state_channel, unsubscribe, id, numbers, wg, descr, true)
			}
			if debug {
				log.Printf("Subscribed: %v", id)
			}
			id += 1
		case unsubscribe_id := <-unsubscribe:
			priority := threads[unsubscribe_id].priority
			delete(priorities_and_threads[priority], unsubscribe_id)
			if len(priorities_and_threads[priority]) == 0 {
				delete(priorities_and_threads, priority)
				max_priority = get_max_priority()
				set_channels(max_priority, true)
			}
		}
	}
}

func parse_line(line string) (string, []int32, int) {
	words := strings.Split(line, " ")
	rpc := words[0]
	priority_str := words[2]
	priority, _ := strconv.Atoi(priority_str)
	numbers_str := strings.Split(words[1], ",")
	var numbers = []int32{}

	for _, i := range numbers_str {
		j, err := strconv.Atoi(i)
		if err != nil {
			panic(err)
		}
		numbers = append(numbers, int32(j))
	}
	return rpc, numbers, priority
}

func count_lines(path_to_file string) int {
	lines := 0
	file, err := os.Open("greeter_client/input_file.txt")
	if err != nil {
		log.Fatal(err)
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)

	const maxCapacity int = 100 // your required line length
	buf := make([]byte, maxCapacity)
	scanner.Buffer(buf, maxCapacity)

	for scanner.Scan() {
		lines += 1
	}

	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}

	return lines
}

func main() {
	if len(os.Args) < 3 {
		log.Printf("Usage: go run main.go <input_file> <output_file> [--scheduler] [--debug]")
		return
	}
	if len(os.Args) > 3 && os.Args[3] == "--scheduler" {
		use_scheduler = true
	}
	if len(os.Args) > 3 && os.Args[3] == "--debug" || len(os.Args) > 4 && os.Args[4] == "--debug" {
		debug = true
	}
	input_file = os.Args[1]
	output_file = os.Args[2]
	if err := os.Truncate(output_file, 0); err != nil {
		log.Printf("Failed to truncate: %v", err)
	}
	var wg sync.WaitGroup
	wg.Add(count_lines(input_file))
	subscribe := make(chan SubscribeElement, 100)

	go scheduler(subscribe, &wg)

	file, err := os.Open(input_file)
	if err != nil {
		log.Fatal(err)
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)

	const maxCapacity int = 100 // your required line length
	buf := make([]byte, maxCapacity)
	scanner.Buffer(buf, maxCapacity)

	start := time.Now()
	for scanner.Scan() {
		// time.Sleep(300 * time.Microsecond)
		descr := scanner.Text()
		var _, numbers, priority = parse_line(descr)
		subscribe <- SubscribeElement{priority: priority, numbers: numbers, descr: descr}
	}

	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}

	wg.Wait()

	elapsed := time.Since(start)
	microseconds_elapsed := elapsed.Microseconds()
	write_to_file(microseconds_elapsed, "Total_time:")
}
