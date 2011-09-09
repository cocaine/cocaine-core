# coding: utf-8

from __future__ import with_statement

import os
import pwd
import zmq
import simplejson
from M2Crypto import EVP

class Executer(object):
    def __init__(self):
        self.context = zmq.Context()
        try:
            self.user = pwd.getpwuid(os.getuid())
        except:
            raise RuntimeError("Can not get current user's passwd entry")

        try:
            self.username = self.user.pw_name
        except:
            raise RuntimeError("Cannot determine your username")

        try:
            path = os.path.join(self.user.pw_dir, ".yappi", "key.pem")
            self.pk = EVP.load_key(path)
        except:
            raise RuntimeError("Cannot load your private key from %s" % key_path)

        try:
            path = os.path.join(self.user.pw_dir, ".yappi", "aliases.json")
            with open(path, 'r') as file:
                self.aliases = simplejson.load(file)
        except:
            self.aliases = {}

    def do(self, uri, hosts, isolated = True):
        if not uri:
            raise ValueError("No target has been specified")

        if not hosts:
            raise ValueError("No hosts has been specified")

        sockets = []

        for host in hosts:
            socket = self.context.socket(zmq.REQ)
            socket.connect("tcp://%s:5000" % host)
            sockets.append(socket)

        if uri in self.aliases:
            uri = self.aliases[uri]

        request = simplejson.dumps({
            'version': 3,
            'token': self.username,
            'action': 'push',
            'targets': {
                uri: {
                    'driver': 'once',
                    'isolated': isolated
                }
            }
        })

        self.pk.sign_init()
        self.pk.sign_update(request)
        signature = self.pk.sign_final()

        [socket.send_multipart([request, signature]) for socket in sockets]
        results = [socket.recv_json()[uri] for socket in sockets]
        [socket.close() for socket in sockets]

        return results
