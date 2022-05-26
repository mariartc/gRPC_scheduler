# gRPC_scheduler

## Diploma thesis 2022
This project was developed for the purposes of my Diploma Thesis in the School of Electrical and Computer Engineering of National Technical University of Athens.

## Overview
gRPC (google RPC) is an open-source RPC framework based on Google Protocol Buffers that uses HTTP/2 connections. It has many advantages concerning the implementation and maintenance, as well as the performance of a system. Purpose of this project was to examine and test the functionality and performance of the gRPC framework, and mainly to develop a software component on the client side, to prioritize rpc calls over a gRPC channel.

## Files description
#### client/
This folder contains various implementations of the gRPC client:
* python client: this is an initial client implementation to test multithreading in a gRPC client
* C++ client: there are three client implementations alongside their scheduler components. The first implementation uses C++ processes to make rpc calls, while the other two use threads. The third implementation also uses a time-out mechanism to avoid starvation.
* Golang client: there are two client implemenations alongside their scheduler components. In the first one, rpc calls of the same priority run serially, while in the second, rpc calls of the same priority are executed concurrently.

#### ghz_plots/
This folder contains all the diagrams with the results of the benchmark tests run using the [ghz](https://ghz.sh/) tool. These diagrams show to impact of a number of parameters used on a gRPC client making rpc calls, on metrics of the performance of the gRPC framework. More specifically, these parameters are:
* the number of threads used by the client to make rpc calls
* the number of cpu cores used by the client application
* the total number of rpc calls made
* the data on the gRPC messages of the calls

while the metrics tested are:
* the average latency of a rpc call
* the number of requests per second
* the total time of the test
* the total time divided by the number of calls made
#### python_scripts/
This folder contains scripts written in python to:
* automate the execution of the ghz benchmarks, and create the appropriate plots using the results
* generate the input file used by the gRPC client to make rpc calls, and create plots using the measurements of the scheduler tests
#### server/
This folder contains an initial implementation of a node.js server that implements only part of the gRPC methods descripted in ```helloworld.proto```, as well as the final gRPC server, written in Golang, which implements all of the methods of the .proto file.
#### helloworld.proto
This is the .proto file that contains the description of all services, methods and messages used on the project based on [Protocol Buffers](https://developers.google.com/protocol-buffers).



