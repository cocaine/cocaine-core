#!/usr/bin/env python

import zmq
import json
import os
from sys import argv
from pprint import pprint


def main(manifests):
    context = zmq.Context()
    request = context.socket(zmq.REQ)
    request.connect('tcp://localhost:5000')

    # Creating the engines
    request.send_json({
        'version': 2,
        'action': 'create',
        'apps': manifests
    })

    pprint(request.recv_json())


if __name__ == "__main__":
    if len(argv) == 1:
        print "Usage: %s <path-to-manifest-1> ... <path-to-manifest-N>" % argv[0]
    else:
        manifests = {}

        for path in argv[1:]:
            with open(path, 'r') as stream:
                filename = os.path.basename(path)
                (appname, _) = os.path.splitext(filename)

                manifests[appname] = json.load(stream)

        main(manifests)
