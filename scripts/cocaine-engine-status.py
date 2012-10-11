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

def main(app, hosts):
    context = zmq.Context()

    for host in hosts:
        request = context.socket(zmq.REQ)
        request.connect('tcp://%s:5000' % host)

        # Statistics
        request.send_json({
            'version': 2,
            'action': 'info'
        })

        res=request.recv_json()

        if app not in res["apps"]:
            print "Engine %s was not found on %s" % (app,host)
            exit(1)

        if res["apps"][app]["state"] != "running":
           print "Engine %s is not running on %s" % (app,host)
           exit(1)

    exit(0)

if __name__ == "__main__":
    if len(argv) == 1:
        print "Fuck off"
    elif len(argv) == 2:
        main(argv[1], ['localhost'])
    else:
        main(argv[1], argv[2:])


