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

from sys import argv
from pprint import pprint

import zmq
import msgpack

SLOT_START_APP = 0
SLOT_PAUSE_APP = 1
SLOT_INFO      = 2

def main(hosts):
    context = zmq.Context()

    for host in hosts:
        request = context.socket(zmq.DEALER)
        request.connect('tcp://%s:5000' % host)

        # Statistics
        request.send(msgpack.packb([SLOT_INFO, []]))

        pprint(msgpack.unpackb(request.recv()))

if __name__ == "__main__":
    if len(argv) == 1:
        main(['localhost'])
    else:
        main(argv[1:])
