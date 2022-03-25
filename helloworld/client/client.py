from __future__ import print_function

import argparse
import logging

import grpc
import helloworld_pb2
import helloworld_pb2_grpc


def generate_numbers(numbers):
    for number in numbers:
        print(f"Sending {number}")
        yield helloworld_pb2.IntNumber(value=number)


def guide_compute_mean_stream(stub, numbers):
    responses = stub.ComputeMeanStream(generate_numbers(numbers))
    for response in responses:
        print(f"Received mean: {response.value}")


def guide_compute_mean(stub, numbers):
    response = stub.ComputeMean(generate_numbers(numbers))
    print(f"Received mean: {response.value}")


def run(args):
    channel = grpc.insecure_channel('localhost:50051')
    stub = helloworld_pb2_grpc.GreeterStub(channel)

    if args.which == 'greeter':
        response = stub.SayHello(helloworld_pb2.HelloRequest(name=args.name))
        print("Greeter client received: " + response.message)

    if args.which == 'compute_mean_stream':
        guide_compute_mean_stream(stub, list(args.numbers))

    if args.which == 'compute_mean':
        guide_compute_mean(stub, list(args.numbers))


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


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='CLI tool to manage\
                                user\'s inputs')

    add_arguments(parser)
    args = parser.parse_args()
                          
    logging.basicConfig()
    run(args)
