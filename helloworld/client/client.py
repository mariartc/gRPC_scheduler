from __future__ import print_function

import argparse
import logging

import grpc
import helloworld_pb2
import helloworld_pb2_grpc

import os
import threading
import time


def generate_numbers(numbers, thread_name, prnt = True):
    for number in numbers:
        if prnt:
            print(f"Thread {thread_name} sending {number}")
        # time.sleep(2)
        yield helloworld_pb2.IntNumber(value=number)


def guide_compute_mean_stream(stub, numbers, thread_name):
    responses = stub.ComputeMeanStream(generate_numbers(numbers, thread_name))
    for response in responses:
        print(f"Thread {thread_name} received mean: {response.value}")


def guide_compute_mean(stub, numbers, thread_name):
    response = stub.ComputeMean(generate_numbers(numbers, thread_name))
    print(f"Thread {thread_name} received mean: {response.value}")


def run_args(args):
    channel = grpc.insecure_channel('localhost:50051')
    stub = helloworld_pb2_grpc.GreeterStub(channel)

    if args.which == 'greeter':
        response = stub.SayHello(helloworld_pb2.HelloRequest(name=args.name))
        print("Greeter client received: " + response.message)

    if args.which == 'compute_mean_stream':
        guide_compute_mean_stream(stub, list(args.numbers))

    if args.which == 'compute_mean':
        guide_compute_mean(stub, list(args.numbers))

    if args.which == 'throughput':
        run_throughput()

    else:
        run_threads()


def task1(stub):
    guide_compute_mean(stub, [i for i in range(10)], threading.current_thread().name)


def task2(stub):
    guide_compute_mean(stub, [i for i in range(15)], threading.current_thread().name)


def task3(stub):
    response = stub.SayHello(helloworld_pb2.HelloRequest(name="you"))
    print(f"Thread {threading.current_thread().name} received: {response.message}")


def run_threads():
    channel = grpc.insecure_channel('localhost:50051')
    stub = helloworld_pb2_grpc.GreeterStub(channel)

    t1 = threading.Thread(target=task1, args=[stub], name='t1')
    t2 = threading.Thread(target=task2, args=[stub], name='t2')
    t3 = threading.Thread(target=task3, args=[stub], name='t3')

    t1.start()
    t2.start()
    t3.start()
    # t1.join()
    # t2.join()
    # t3.join()


def run_throughput():
    channel = grpc.insecure_channel('localhost:50051')
    stub = helloworld_pb2_grpc.GreeterStub(channel)

    response = stub.ComputeMean(generate_numbers([3 for _ in range(100000)], "main", False))
    print(f"Thread main received mean: {response.value}")



def add_arguments(parser: argparse.ArgumentParser):
    sub = parser.add_subparsers(help='Operation to execute')

    parser_add_node = sub.add_parser('greeter', help='Send greetings to server')
    parser_add_node.set_defaults(which='greeter')
    parser_add_node.add_argument('-n', '--name', default=-1, type=str,
                                 help='Your name')

    parser_transaction = sub.add_parser('mean_stream',
                                        help='Compute mean value gradually')
    parser_transaction.set_defaults(which='compute_mean_stream')
    parser_transaction.add_argument('-n', '--numbers', nargs="+", default=-1, type=int,
                                 help='The numbers of which mean value will be computed gradually')

    parser_view = sub.add_parser('mean', help='Compute mean value')
    parser_view.set_defaults(which='compute_mean')
    parser_view.add_argument('-n', '--numbers', nargs="+", default=-1, type=int,
                                 help='The numbers of which mean value will be computed')

    parser_add_node = sub.add_parser('throughput', help='Test server\'s packets/sec')
    parser_add_node.set_defaults(which='throughput')

    parser_add_node = sub.add_parser('threads', help='Test threads')
    parser_add_node.set_defaults(which='threads')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='CLI tool to manage\
                                user\'s inputs')


    logging.basicConfig()
    add_arguments(parser)
    args = parser.parse_args()
    run_args(args)