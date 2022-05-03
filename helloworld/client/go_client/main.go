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
	addr          = flag.String("addr", "localhost:50051", "the address to connect to")
	debug         = false
	input_file    = ""
	output_file   = ""
	threads       = make(map[int]Thread)
	use_scheduler = false
	mutex         sync.Mutex
)

type Thread struct {
	priority int
	channel  chan bool
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

func thread_without_scheduler(numbers []int32, wg *sync.WaitGroup, descr string) {
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

	for _, number := range numbersInt {
		if err := stream.Send(number); err != nil {
			log.Fatalf("%v.Send(%v) = %v", stream, number, err)
		}
	}
	reply, err := stream.CloseAndRecv()
	if err != nil {
		log.Fatalf("%v.ComputeMean() got error %v, want %v", stream, err, nil)
	}
	elapsed := time.Since(start)
	microseconds_elapsed := elapsed.Microseconds()
	go write_to_file(microseconds_elapsed, descr)
	if debug {
		log.Printf("Got mean: %v", reply.Value)
	}
	wg.Done()
}

func thread(state <-chan bool, unsubscribe chan<- int, id int, numbers []int32, wg *sync.WaitGroup, descr string) {
	if debug {
		log.Printf("Thread %v going to sleep...", id)
	}

	conn, err := grpc.Dial(*addr, grpc.WithInsecure())
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	client := pb.NewGreeterClient(conn)

	var active = false
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
	microseconds_elapsed := elapsed.Microseconds()
	write_to_file(microseconds_elapsed, descr)
	if debug {
		log.Printf("Thread %v Got mean: %v", id, reply.Value)
	}
	unsubscribe <- id
	wg.Done()
}

func get_max_priority() int {
	if len(threads) == 0 {
		return -1
	}

	max_priority := 0
	max_id := -1
	for id, thread := range threads {
		if thread.priority > max_priority || thread.priority == max_priority && id < max_id {
			max_id = id
			max_priority = thread.priority
		}
	}

	return max_id
}

func scheduler(subscribe <-chan string, wg *sync.WaitGroup) {
	unsubscribe := make(chan int, 20)
	max_id := -1
	running_id := -1
	id := 1

	for {
		select {
		case subscribe_str := <-subscribe:
			var _, numbers, priority = parse_line(subscribe_str)
			state_channel := make(chan bool, 2)
			threads[id] = Thread{priority: priority, channel: state_channel}
			go thread(state_channel, unsubscribe, id, numbers, wg, subscribe_str)

			if max_id == -1 || priority > threads[max_id].priority {
				max_id = id
				if running_id != -1 {
					threads[running_id].channel <- false
				}
				threads[max_id].channel <- true
				running_id = max_id
			}
			if debug {
				log.Printf("Subscribed: %v", id)
			}
			id += 1
		case unsubscribe_id := <-unsubscribe:
			delete(threads, unsubscribe_id)
			running_id = -1
			max_id = get_max_priority()
			if debug {
				log.Printf("Unsubscribed: %v, new max_id = %v", unsubscribe_id, max_id)
			}
			if max_id != -1 {
				running_id = max_id
				threads[max_id].channel <- true
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
	var wg sync.WaitGroup
	wg.Add(count_lines(input_file))
	subscribe := make(chan string, 20)

	if use_scheduler {
		go scheduler(subscribe, &wg)
	}

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
		if use_scheduler {
			subscribe <- scanner.Text()
		} else {
			descr := scanner.Text()
			var _, numbers, _ = parse_line(descr)
			go thread_without_scheduler(numbers, &wg, descr)
		}
	}

	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}

	wg.Wait()

	elapsed := time.Since(start)
	microseconds_elapsed := elapsed.Microseconds()
	write_to_file(microseconds_elapsed, "Total_time:")
}
