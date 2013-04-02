#!/usr/bin/env python
#
#    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
#    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.
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

from sys import argv
from pprint import pprint

import zmq
import msgpack

SLOT_START_APP = 0
SLOT_PAUSE_APP = 1
SLOT_INFO      = 2

def main(apps):
    context = zmq.Context()
    
    request = context.socket(zmq.DEALER)
    request.connect('tcp://localhost:5000')

    # Pausing the apps
    request.send_multipart([
        msgpack.packb(SLOT_PAUSE_APP),
        msgpack.packb([apps])
    ])

    pprint(msgpack.unpackb(request.recv()))

if __name__ == "__main__":
    if len(argv) == 1:
        print "USAGE: %s <app-name-1> ... <app-name-N>" % argv[0]
    else:
        main(argv[1:])
