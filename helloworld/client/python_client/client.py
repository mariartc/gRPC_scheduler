from __future__ import print_function
from arg_parser import add_arguments
# from grpc_interceptor import UnaryUnaryClientInterceptor

import argparse
import logging

import grpc
import helloworld_pb2
import helloworld_pb2_grpc
import signal
import threading
import time


def generate_long_string():
    # long_string = ""
    # for _ in range(4194290):
    #     long_string += 'a'
    # file = open("client/long_string.txt","w")
    # file.write(long_string)
    # file.close()
    file = open("client/long_string.txt","r+")
    return file.read()


def generate_numbers(numbers, thread_name = "main", prnt = True):
    for number in numbers:
        if prnt:
            print(f"Thread {thread_name} sending {number}")
        # time.sleep(2)
        yield helloworld_pb2.IntNumber(value=number)


def generate_float_or_long_string(long_str = None, numbers = None):
    if long_str:
        yield helloworld_pb2.FloatOrLongString(long_string_type=helloworld_pb2.LongString(str=long_str))
    else:
        for number in numbers:
            yield helloworld_pb2.FloatOrLongString(float_number_type=helloworld_pb2.FloatNumber(value=number))


def compute_mean_stream(stub, numbers, thread_name = "main"):
    responses = stub.ComputeMeanStream(generate_numbers(numbers, thread_name))
    for response in responses:
        print(f"Thread {thread_name} received mean: {response.value}")


def compute_mean(stub, numbers, thread_name = "main", prnt = True):
    response = stub.ComputeMean(generate_numbers(numbers, thread_name, prnt))
    if prnt:
        print(f"Thread {thread_name} received mean: {response.value}")


def send_long_string(stub):
    long_string = generate_long_string()
    stub.SendLongString(helloworld_pb2.LongString(str=long_string))


def compute_mean_or_send_long_string(stub, which):
    if which == "send_long_string":
        long_string = generate_long_string()
        stub.ComputeMeanOrSendLongString(generate_float_or_long_string(long_str=long_string))
    else:
        stub.ComputeMeanOrSendLongString(generate_float_or_long_string(numbers=[3 for _ in range(5)]))


# THREAD FUNCTIONS
def task1(stub):
    compute_mean(stub, [3 for i in range(10)], threading.current_thread().name)


def task2(stub):
    compute_mean(stub, [i for i in range(15)], threading.current_thread().name)


def task3(stub):
    response = stub.SayHello(helloworld_pb2.HelloRequest(name="you"))
    print(f"Thread {threading.current_thread().name} received: {response.message}")


def task4(stub):
    start = time.time()
    send_long_string(stub)
    end = time.time()
    print(f"SendLongString RPC execution time from {threading.current_thread().name}: {end - start}")


def task5(stub):
    start = time.time()
    compute_mean(stub, [3 for _ in range(5)], prnt = False)
    end = time.time()
    print(f"ComputeMean RPC execution time from {threading.current_thread().name}: {end - start}")


def task6(stub):
    start = time.time()
    compute_mean_or_send_long_string(stub, "send_long_string")
    end = time.time()
    print(f"SendLongString with common RPC execution time from {threading.current_thread().name}: {end - start}")


def task7(stub):
    start = time.time()
    compute_mean_or_send_long_string(stub, "compute_mean")
    end = time.time()
    print(f"ComputeMean with common RPC execution time from {threading.current_thread().name}: {end - start}")


# RUN FUNCTIONS
def run_threads(stub):
    t1 = threading.Thread(target=task1, args=[stub], name='t1')
    t2 = threading.Thread(target=task2, args=[stub], name='t2')
    t3 = threading.Thread(target=task3, args=[stub], name='t3')

    t1.start()
    t2.start()
    t3.start()


def run_throughput(stub):
    response = stub.ComputeMean(generate_numbers([3 for _ in range(100000)], "main", False))
    print(f"Thread main received mean: {response.value}")


def run_serially(stub):
    start = time.time()
    compute_mean(stub, [3 for _ in range(5)], prnt = False)
    end = time.time()
    print(f"ComputeMean RPC execution time: {end - start}")
    start = time.time()
    send_long_string(stub)
    end = time.time()
    print(f"SendLongString RPC execution time: {end - start}")


def run_concurrently(stub):
    t4 = threading.Thread(target=task4, args=[stub], name='t4')
    t5 = threading.Thread(target=task5, args=[stub], name='t5')

    t4.start()
    t5.start()


def run_serially_common_rpc(stub):
    start = time.time()
    compute_mean_or_send_long_string(stub, "compute_mean")
    end = time.time()
    print(f"ComputeMean with common RPC execution time: {end - start}")
    start = time.time()
    compute_mean_or_send_long_string(stub, "send_long_string")
    end = time.time()
    print(f"SendLongString with common RPC execution time for : {end - start}")


def run_concurrently_common_rpc(stub):
    t6 = threading.Thread(target=task6, args=[stub], name='t6')
    t7 = threading.Thread(target=task7, args=[stub], name='t7')

    t6.start()
    t7.start()


def run_args(args):
    options = [('grpc.max_message_length', 100 * 1024 * 1024)]
    channel = grpc.insecure_channel('127.0.0.1:50051', options)
    stub = helloworld_pb2_grpc.GreeterStub(channel)

    if args.which == 'greeter':
        response = stub.SayHello(helloworld_pb2.HelloRequest(name=args.name))
        print("Greeter client received: " + response.message)
    elif args.which == 'compute_mean_stream':
        compute_mean_stream(stub, list(args.numbers))
    elif args.which == 'compute_mean':
        compute_mean(stub, list(args.numbers))
    elif args.which == 'throughput':
        run_throughput(stub)
    elif args.which == 'threads':
        run_threads(stub)
    elif args.which == 'serial':
        run_serially(stub)
    elif args.which == 'concurrent':
        run_concurrently(stub)
    elif args.which == 'serial_common_rpc':
        run_serially_common_rpc(stub)
    elif args.which == 'concurrent_common_rpc':
        run_concurrently_common_rpc(stub)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='CLI tool to manage\
                                user\'s inputs')


    logging.basicConfig()
    add_arguments(parser)
    args = parser.parse_args()
    run_args(args)
