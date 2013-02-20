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

from pprint import pprint
import socket
from sys import argv

import msgpack

SLOT_START_APP = 0
SLOT_PAUSE_APP = 1
SLOT_INFO      = 2

def main(hosts):
    for host in hosts:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        sock.connect(('127.0.0.1', 12350))

        # Statistics
        sock.send(msgpack.packb([SLOT_INFO, []]))
        res = sock.recv(512)
        pprint(msgpack.unpackb(res))

if __name__ == "__main__":
    if len(argv) == 1:
        main(['localhost'])
    else:
        main(argv[1:])
