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

import errno
import json
import tarfile
from pprint import pprint
from optparse import OptionParser
import sys

import msgpack

from cocaine.service.services import Service

DESCRIPTION = ""
ACTIONS=("app:list", "app:upload",  "app:remove",\
            "profile:upload", "profile:remove", "profile:list", "profile:view",\
            "runlist:list", "runlist:remove", "runlist:upload", "runlist:view")
USAGE="Usage: %prog " + "%s <options>" % '|'.join(ACTIONS)

DEFAULT_HOST="localhost"
DEFAULT_PORT=10053

class Storage(object):

    def __init__(self, hostname, port):
        self._st = Service("storage", hostname, port)

    def _list(self, namespace):
        return self._st.perform_sync("list", namespace)

    def apps(self):
        print "Currently uploaded apps:"
        for app in self._list("manifests"):
            print "\t" + app
        exit(0)

    def upload(self, manifest_path, archive_path, name):
        # Validate manifest
        try:
            with open(manifest_path,'rb') as manifest_file:
                manifest = manifest_file.read()
                manifest = json.loads(manifest)
                manifest = msgpack.packb(manifest)
        except IOError as err:
            print "Error: unable to open manifest file %s." % manifest_path
            exit(1)
        except ValueError as err:
            print "Error: the app manifest in %s is corrupted." % manifest_path
            exit(1)

        # Validate application archive
        try:
            if not tarfile.is_tarfile(archive_path):
                print "Error: Wrong archive file %s" % archive_path
                exit(1)
            else:
                with open(archive_path, 'rb') as archive:
                    blob = msgpack.packb(archive.read())
        except IOError as err:
            print "Error: no such archive file %s." % (archive_path)
            exit(1)

        # Upload
        try:
            self._st.perform_sync("write", "manifests", name, manifest)
            self._st.perform_sync("write", "apps", name, blob)
        except Exception as err:
            print "Error: unable to upload app. %s" % str(err)
            exit(1)
        else:
            print "The %s app has been successfully uploaded." % name
            exit(0)

    def remove(self, name):
        if not name in self._list("apps"):
            print "app %s is not uploaded" % name
            exit(0)
        try:
            self._st.perform_sync("remove", "manifests", name)
            self._st.perform_sync("remove", "apps", name)
        except Exception as err:
            print "Error: unable to remove app. %s" % str(err)
            exit(1)
        else:
            print "The %s app has been successfully removed." % name
            exit(0)

    def profiles(self):
        print "Currently uploaded profiles:"
        for profile in self._list("profiles"):
            print "\t" + profile
        exit(0)

    def upload_profile(self, profile_path, name):
        try:
            with open(profile_path,'rb') as profile_file:
                profile = profile_file.read()
                profile = json.loads(profile)
                profile = msgpack.packb(profile)
        except IOError as err:
            print "Error: unable to open profile file %s." % profile_path
            exit(1)
        except ValueError as err:
            print "Error: the app profile in %s is corrupted." % profile_path
            exit(1)

        try:
            self._st.perform_sync("write", "profiles", name, profile)
        except Exception as err:
            print "Error: unable to upload profile. %s" % str(err)
            exit(1)
        else:
            print "The %s profile has been successfully uploaded." % name
            exit(0)

    def remove_profile(self, name):
        if not name in self._list("profiles"):
            print "profile %s is not uploaded" % name
            exit(0)
        try:
            self._st.perform_sync("remove", "profiles", name)
        except Exception as err:
            print "Error: unable to remove profile. %s" % str(err)
            exit(1)
        else:
            print "The %s profile has been successfully removed." % name
            exit(0)

    def view_profile(self, name):
        try:
            pprint(msgpack.unpackb(self._st.perform_sync("read", "profiles", name)))
        except Exception as err:
            print "Error: unable to view profile. %s" % str(err)
            exit(1)
        exit(0)

    def runlists(self):
        print "Currently uploaded runlists:"
        for profile in self._list("runlists"):
            print "\t" + profile
        exit(0)

    def view_runlist(self, name):
        try:
            pprint(msgpack.unpackb(self._st.perform_sync("read", "runlists", name)))
        except Exception as err:
            print "Error: unable to view runlist. %s" % str(err)
            exit(1)
        exit(0)

    def upload_runlist(self, runlist_path, name):
        try:
            with open(runlist_path,'rb') as runlist_file:
                runlist = runlist_file.read()
                runlist = json.loads(runlist)
                runlist = msgpack.packb(runlist)
        except IOError as err:
            print "Error: unable to open runlist file %s." % runlist_path
            exit(1)
        except ValueError as err:
            print "Error: the app runlist in %s is corrupted." % runlist_path
            exit(1)

        try:
            self._st.perform_sync("write", "runlists", name, runlist)
        except Exception as err:
            print "Error: unable to upload runlist. %s" % str(err)
            exit(1)
        else:
            print "The %s runlist has been successfully uploaded." % name
            exit(0)

    def remove_runlist(self, name):
        if not name in self._list("runlists"):
            print "runlist %s is not uploaded" % name
            exit(0)
        try:
            self._st.perform_sync("remove", "runlists", name)
        except Exception as err:
            print "Error: unable to remove runlist. %s" % str(err)
            exit(1)
        else:
            print "The %s runlist has been successfully removed." % name
            exit(0)


def main():
    parser = OptionParser(usage=USAGE, description=DESCRIPTION)
    parser.add_option("-m",  type = "str", dest="manifest", help="location of the app manifest or runlist, profile", metavar="manifest.json")
    parser.add_option("-n",  type = "str", dest="name", help="name of the app or profile", metavar="examplename")
    parser.add_option("-p",  type = "str", dest="package", help="location of the app source package", metavar="package.tar.gz")
    parser.add_option("--port", type = "int", default=DEFAULT_PORT, help="Port number\t [default: %default]", metavar=DEFAULT_PORT)
    parser.add_option("--host", type = "str", default=DEFAULT_HOST, help="Hostname\t [default: %default]", metavar=DEFAULT_HOST)
    (options, args) = parser.parse_args()

    if len(args) == 0:
        parser.print_usage()
        print "Empty action. Use %s" % '|'.join(ACTIONS)
        exit(0)
    else:
        S = Storage(options.host, options.port)
        action = args[0]
        # Operation with applications
        if action == "app:list":
            S.apps()

        elif action == "app:upload":
            if options.name is not None and options.manifest is not None and options.package is not None:
                S.upload(options.manifest, options.package, options.name)
            else:
                print "Specify name, manifest, package of the app"

        elif action == "app:remove":
            if options.name is not None:
                S.remove(options.name)
            else:
                print "Empty application name"

        # Operations with profiles
        elif action == "profile:list":
            S.profiles()

        elif action == "profile:upload":
            if options.name is not None and options.manifest is not None:
                S.upload_profile(options.manifest, options.name)
            else:
                print "Specify the name of the profile and profile filepath"

        elif action == "profile:remove":
            if options.name is not None:
                S.remove_profile(options.name)
            else:
                print "Empty profile name"

        elif action == "profile:view":
            if options.name is not None:
                S.view_profile(options.name)
            else:
                print "Empty profile name"

        # Operations with runlists
        elif action == "runlist:list":
            S.runlists()

        elif action == "runlist:upload":
            if options.name is not None and options.manifest is not None:
                S.upload_runlist(options.manifest, options.name)
            else:
                print "Specify the name of the runlist and profile filepath"

        elif action == "runlist:remove":
            if options.name is not None:
                S.remove_runlist(options.name)
            else:
                print "Empty runlist name"

        elif action == "runlist:view":
            if options.name is not None:
                S.view_runlist(options.name)
            else:
                print "Empty runlist name"
        else:
            print "Invalid action %s. Use %s" % (action, '|'.join(ACTIONS))

            exit(0)

if __name__ == "__main__":
    main()
