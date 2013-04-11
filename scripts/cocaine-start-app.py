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
from cocaine.exceptions import ServiceError

DESCRIPTION=""
USAGE="USAGE: %prog <app1>@<profile1> <app2>@<profile2> ... <appN>@<profileN> --host [host] --port [port]"



def main(apps, hostname, port):

    try:
        runlist = dict(app.split('@') for app in apps)
    except ValueError:
        print "ERROR: Not all apps have their profiles specified."
        sys.exit(1)

    if not all(runlist):
        print "ERROR: Not all apps have valid names."
        sys.exit(1)

    node = Service("node", hostname, port)
    try:
        pprint(node.perform_sync("start_app", runlist))
    except ServiceError as err:
        print err
        sys.exit(1)


if __name__ == "__main__":
    parser = OptionParser(usage=USAGE, description=DESCRIPTION)
    parser.add_option("--port", type = "int", default=10053, help="Port number")
    parser.add_option("--host", type = "str", default="localhost", help="Hostname")

    (options, args) = parser.parse_args()
    if len(args) == 0:
        print "Specify applications and profiles"
        parser.print_usage()
        parser.print_help()
        sys.exit(1)
    try:
        main(args, options.host, options.port)
    except Exception as err:
        if err.args[0] == errno.ECONNREFUSED:
            print "Invalid endpoint: %s:%d" % (options.host, options.port)
            parser.print_usage()
            parser.print_help()
            sys.exit(1)
        else:
            print err
