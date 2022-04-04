import argparse

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
    
    parser_add_node = sub.add_parser('serial', help='Run two rpc\'s serially')
    parser_add_node.set_defaults(which='serial')
    
    parser_add_node = sub.add_parser('concurrent', help='Run two rpc\'s concurrently')
    parser_add_node.set_defaults(which='concurrent')
    
    parser_add_node = sub.add_parser('serial_common_rpc', help='Run one common rpc serially')
    parser_add_node.set_defaults(which='serial_common_rpc')
    
    parser_add_node = sub.add_parser('concurrent_common_rpc', help='Run one common rpc concurrently')
    parser_add_node.set_defaults(which='concurrent_common_rpc')
