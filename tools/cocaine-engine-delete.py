#!/usr/bin/env python

import zmq
from sys import argv
from pprint import pprint


def main(apps):
    context = zmq.Context()
    
    request = context.socket(zmq.REQ)
    request.connect('tcp://localhost:5000')

    request.send_json({
        'version': 2,
        'action': 'delete',
        'apps': apps
    })

    pprint(request.recv_json())


if __name__ == "__main__":
    if len(argv) == 1:
        print "Usage: %s <app-name-1> ... <app-name-N>" % argv[0]
    else:
        main(argv[1:])
