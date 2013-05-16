#!/usr/bin/env python
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
from optparse import OptionParser

import msgpack

from cocaine.services import Service

DESCRIPTION = ""
ACTIONS=("app:list", "app:view", "app:upload",  "app:remove",\
            "profile:upload", "profile:remove", "profile:list", "profile:view",\
            "runlist:list", "runlist:remove", "runlist:upload", "runlist:view")
USAGE="Usage: %prog " + "%s <options>" % '|'.join(ACTIONS)

DEFAULT_HOST = "localhost"
DEFAULT_PORT = 10053
DEFAULT_TIMEOUT = 2

APPS_TAGS = ("APPS",)
RUNLISTS_TAGS = ("RUNLISTS",)
PROFILES_TAGS = ("PROFILES",)

def sync_decorator(func, timeout):
    def wrapper(*args, **kwargs):
        try:
            res = ""
            info = func(timeout = kwargs.get("timeout") or timeout, *args, **kwargs)
            res = info.next()
            info.next()
        except StopIteration:
            return res
    return wrapper

def exists(namespace, tag, entity_name=""):
    def decorator(func):
        def wrapper(self, *args, **kwargs):
            name = args[0]
            if not name in self._list(namespace, tag):
                print "%s %s is not uploaded" % (entity_name, name)
                exit(0)
            return func(self, *args, **kwargs)
        return wrapper
    return decorator

def not_exists(namespace, tag, entity_name=""):
    def decorator(func):
        def wrapper(self, *args, **kwargs):
            name = args[0]
            if name in self._list(namespace, tag):
                print "%s %s has been already uploaded" % (entity_name, name)
                exit(0)
            return func(self, *args, **kwargs)
        return wrapper
    return decorator

class Sync_wrapper(object):

    def __init__(self, obj, timeout):
        self._obj = obj
        self._timeout = timeout

    def __getattr__(self, name):
        _async = getattr(self._obj, name)
        return sync_decorator(_async, self._timeout)

def print_json(data):
    print json.dumps(data, indent=2)

class Storage(object):

    def __init__(self, hostname, port, timeout):
        self._st = Sync_wrapper(Service("storage", hostname, port), timeout)

    def _list(self, namespace, tags):
        return self._st.perform_sync("find", namespace, tags)

    def apps(self):
        print "Currently uploaded apps:"
        for n, app in enumerate(self._list("manifests", APPS_TAGS)):
            print "\t%d. %s" % (n + 1, app)
        exit(0)

    @exists("manifests", APPS_TAGS, "manifest")
    @exists("apps", APPS_TAGS, "app")
    def view(self, name):
        try:
            print_json(msgpack.unpackb(self._st.perform_sync("read", "manifests", name)))
        except Exception as err:
            print "Error: unable to view application. %s" % str(err)
            exit(1)
        exit(0)

    @not_exists("manifests", APPS_TAGS, "manifest")
    @not_exists("apps", APPS_TAGS, "Source of app")
    def upload(self, name, manifest_path, archive_path):
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
            self._st.perform_sync("write", "manifests", name, manifest, APPS_TAGS)
            self._st.perform_sync("write", "apps", name, blob, APPS_TAGS)
        except Exception as err:
            print "Error: unable to upload app. %s" % str(err)
            exit(1)
        else:
            print "The %s app has been successfully uploaded." % name
            exit(0)

    def remove(self, name):
        try:
            self._st.perform_sync("remove", "manifests", name)
            # Clean symlinks in fs
            self._list("manifests", APPS_TAGS)
        except Exception as err:
            print "Error: unable to remove manifest for app. %s" % str(err)
        else:
            print "The %s manifest of app has been successfully removed." % name

        try:
            self._st.perform_sync("remove", "apps", name)
            # Clean symlinks in fs
            self._list("apps", APPS_TAGS)
        except Exception as err:
            print "Error: unable to remove app source. %s" % str(err)
        else:
            print "The %s source of app has been successfully removed." % name
        exit(0)

    def profiles(self):
        print "Currently uploaded profiles:"
        for n, profile in enumerate(self._list("profiles", PROFILES_TAGS)):
            print  "\t%d. %s" %(n + 1, profile)
        exit(0)

    @not_exists("profiles", PROFILES_TAGS, "profile")
    def upload_profile(self, name, profile_path):
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
            self._st.perform_sync("write", "profiles", name, profile, PROFILES_TAGS)
        except Exception as err:
            print "Error: unable to upload profile. %s" % str(err)
            exit(1)
        else:
            print "The %s profile has been successfully uploaded." % name
            exit(0)

    @exists("profiles", PROFILES_TAGS, "profile")
    def remove_profile(self, name):
        try:
            self._st.perform_sync("remove", "profiles", name)
            # Clear sumlinks in file storage:
            self._list("profiles", PROFILES_TAGS)
        except Exception as err:
            print "Error: unable to remove profile. %s" % str(err)
            exit(1)
        else:
            print "The %s profile has been successfully removed." % name
            exit(0)

    @exists("profiles", PROFILES_TAGS, "profile")
    def view_profile(self, name):
        try:
            print_json(msgpack.unpackb(self._st.perform_sync("read", "profiles", name)))
        except Exception as err:
            print "Error: unable to view profile. %s" % str(err)
            exit(1)
        exit(0)

    def runlists(self):
        print "Currently uploaded runlists:"
        for n, runlist in enumerate(self._list("runlists", RUNLISTS_TAGS)):
            print "\t%d. %s" % (n + 1, runlist)
        exit(0)

    @exists("runlists", RUNLISTS_TAGS, "runlist")
    def view_runlist(self, name):
        try:
            print_json(msgpack.unpackb(self._st.perform_sync("read", "runlists", name)))
        except Exception as err:
            print "Error: unable to view runlist. %s" % str(err)
            exit(1)
        exit(0)

    @not_exists("runlists", RUNLISTS_TAGS, "runlist")
    def upload_runlist(self, name, runlist_path):
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
            self._st.perform_sync("write", "runlists", name, runlist, RUNLISTS_TAGS)
        except Exception as err:
            print "Error: unable to upload runlist. %s" % str(err)
            exit(1)
        else:
            print "The %s runlist has been successfully uploaded." % name
            exit(0)

    @exists("runlists", RUNLISTS_TAGS, "runlist")
    def remove_runlist(self, name):
        try:
            self._st.perform_sync("remove", "runlists", name)
            # Clear sumlinks in file storage:
            self._list("runlists", RUNLISTS_TAGS)
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
    parser.add_option("--timeout",  type = "float", default=DEFAULT_TIMEOUT, help="timeout for synchronous operations [default: %default]", metavar=DEFAULT_TIMEOUT)
    parser.add_option("--port", type = "int", default=DEFAULT_PORT, help="Port number\t [default: %default]", metavar=DEFAULT_PORT)
    parser.add_option("--host", type = "str", default=DEFAULT_HOST, help="Hostname\t [default: %default]", metavar=DEFAULT_HOST)
    (options, args) = parser.parse_args()

    if len(args) == 0:
        parser.print_usage()
        print "Empty action. Use %s" % '|'.join(ACTIONS)
        exit(0)
    else:
        try:
            S = Storage(options.host, options.port, options.timeout)
        except Exception as err:
            if err.args[0] == errno.ECONNREFUSED:
                print "Invalid cocaine-runtime endpoint: %s:%d" % (options.host, options.port)
                exit(1)

        action = args[0]

        # Operation with applications
        if action == "app:list":
            S.apps()

        elif action == "app:view":
            if options.name is not None:
                S.view(options.name)
            else:
                print "Specify name of application"

        elif action == "app:upload":
            if options.name is not None and options.manifest is not None and options.package is not None:
                S.upload(options.name, options.manifest, options.package)
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
                S.upload_profile(options.name, options.manifest)
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
                S.upload_runlist(options.name, options.manifest)
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
            print "Invalid action %s. Use: \n\t%s" % (action, '\n\t'.join(ACTIONS))
            exit(0)

if __name__ == "__main__":
    main()
