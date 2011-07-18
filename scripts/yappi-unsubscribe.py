#!/usr/bin/env python
# coding: utf-8

import zmq
from sys import argv

def unsubscribe(token, target, interval):
    ctx = zmq.Context()
    socket = ctx.socket(zmq.REQ)

    request = {
        'version': 2,
        'token': token,
        'action': 'drop',
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
        print response[target]['result']

if __name__ == '__main__':
    if len(argv) != 4:
        print "Usage: yappi-unsubscribe [token] [key] [interval]"
    else:
        unsubscribe(str(argv[1]), str(argv[2]), int(argv[3]))
