#! /usr/bin/env python
#
#    Copyright (c) 2011-2013 Anton Tyurin <noxiouz@yandex.ru>
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

from pprint import pprint
from optparse import OptionParser
import sys
import errno

from cocaine.service.services import Service

DESCRIPTION=""
USAGE="USAGE: %prog [options]"
DEFAULT_PORT=10053
DEFAULT_HOST="localhost"


def main(hostname, port):
    node = Service("node", hostname, port)
    pprint(node.perform_sync("info"))


if __name__ == "__main__":
    parser = OptionParser(usage=USAGE, description=DESCRIPTION)
    parser.add_option("--port", type = "int", default=DEFAULT_PORT, help="Port number [default: %default]")
    parser.add_option("--host", type = "str", default=DEFAULT_HOST, help="Hostname [default: %default]")
    (options, args) = parser.parse_args()
    try:
        main(options.host, options.port)
    except Exception as err:
        if err.args[0] == errno.ECONNREFUSED:
            print "Invalid endpoint: %s:%d" % (options.host, options.port)
