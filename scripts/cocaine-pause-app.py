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
USAGE="USAGE: %prog --name [APP_NAME] --host [host] --port [port]"



def main(name, hostname, port):
    node = Service("node", hostname, port)
    pprint(node.perform_sync("pause_app", [name]))


if __name__ == "__main__":
    parser = OptionParser(usage=USAGE, description=DESCRIPTION)
    parser.add_option("--port", type = "int", default=10053, help="Port number [default: %default]")
    parser.add_option("--host", type = "str", default="localhost", help="Hostname [default: %default]")
    parser.add_option("-n", "--name", type = "str", help="Application name")

    (options, args) = parser.parse_args()
    if (not options.name):
        parser.print_usage()
        print "Specify application name"
        sys.exit(1)
    try:
        main(options.name, options.host, options.port)
    except Exception as err:
        if err.args[0] == errno.ECONNREFUSED:
            parser.print_usage()
            print "Invalid endpoint: %s:%d. Specify another endpoint by optionts" % (options.host, options.port)
            sys.exit(1)
        else:
            print err
