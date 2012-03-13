#!/usr/bin/env python

import zmq
from sys import argv
from pprint import pprint


def main(hosts):
    context = zmq.Context()

    for host in hosts:
        request = context.socket(zmq.REQ)
        request.connect('tcp://%s:5000' % host)

        # Statistics
        request.send_json({
            'version': 2,
            'action': 'info'
        })

        pprint(request.recv_json())


if __name__ == "__main__":
    if len(argv) == 1:
        main(['localhost'])
    else:
        main(argv[1:])
