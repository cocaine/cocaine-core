#!/usr/bin/env python
#
#    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
#    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.
#
#    This file is part of Cocaine.
#
#    Cocaine is free software; you can redistribute it and/or modify
#    it under the terms of the GNU Lesser General Public License as published by
#    the Free Software Foundation; either version 3 of the License, or
#    (at your option) any later version.
#
#    Cocaine is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#    GNU Lesser General Public License for more details.
#
#    You should have received a copy of the GNU Lesser General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>. 
#

from optparse import OptionParser
from sys import argv, exit

import zmq
import msgpack

SLOT_START_APP = 0
SLOT_PAUSE_APP = 1
SLOT_INFO      = 2

def main(app, hosts, timeout):
    complain = False
    context = zmq.Context()

    for host in hosts:
        request = context.socket(zmq.DEALER)
        request.setsockopt(zmq.LINGER, 0)
        request.connect('tcp://%s:5000' % host)

        # Statistics
        request.send_multipart([
            msgpack.packb(SLOT_INFO),
            msgpack.packb([])
        ])

        poller = zmq.Poller()
        poller.register(request, zmq.POLLIN)

        sockets = dict(poller.poll(timeout = timeout))

        if request in sockets and sockets[request] == zmq.POLLIN:
            response = msgpack.unpackb(request.recv())

            if app not in response["apps"]:
                print "The '%s' app was not found on '%s'." % (app, host)
                exit(1)

            state = response["apps"][app]["state"]
            print "The '%s' app is %s on '%s'." % (app, state, host)
            complain |= (state != "running")
        else:
            print "ERROR: Host '%s' is not responding." % host
            complain |= True

    exit(complain)

description = """This tool allows you to check if the specified app
is up and running on one or multiple nodes. On success you'll get
no output and an exit code of '0', otherwise the error message will
describe the problem, and the exit code will be '1'."""
    
usage = "USAGE: %prog <app-name> [options] [<host-name-1> ... <host-name-N>]"

version = "0.10.0"

if __name__ == "__main__":
    parser = OptionParser(usage = usage,
                          description = description,
                          version = version)

    parser.add_option("-t", "--timeout",
                      type = "int",
                      default = 3000,
                      help = "The amount of time to wait for the node to respond, in milliseconds")
    
    (options, args) = parser.parse_args()

    if len(args) == 0:
        parser.print_usage()
    elif len(args) == 1:
        main(args[0], ['localhost'], options.timeout)
    else:
        main(args[0], args[1:], options.timeout)

