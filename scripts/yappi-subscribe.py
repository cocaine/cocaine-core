#!/usr/bin/env python
# coding: utf-8

import zmq
from sys import argv

def subscribe(target, interval):
    ctx = zmq.Context()
    socket = ctx.socket(zmq.REQ)

    request = {
        'action': 'push',
        'targets': {
            target: {
                'interval': interval
            }
        }
    }

    socket.connect('tcp://localhost:5000')
    socket.send_json(request)
    response = socket.recv_json()

    if "error" in response[target]:
        print response[target]['error']
    else:
        print response[target]['key']

if __name__ == '__main__':
    if len(argv) != 3:
        print "Usage: yappi-subscribe [target] [interval]"
    else:
        subscribe(argv[1], int(argv[2]))
