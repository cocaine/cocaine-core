# coding: utf-8

from __future__ import with_statement

import os
import zmq
import simplejson
from M2Crypto import EVP

class Executer(object):
    def __init__(self):
        self.context = zmq.Context()

        try:
            self.username = os.environ["USER"]
        except:
            raise RuntimeError("Cannot determine your username")

        try:
            path = os.path.join(os.environ["HOME"], ".yappi", "key.pem")
            self.pk = EVP.load_key(path)
        except:
            raise RuntimeError("Cannot load your private key from %s" % key_path)

        try:
            path = os.path.join(os.environ["HOME"], ".yappi", "aliases.json")
            with open(path, 'r') as file:
                self.aliases = simplejson.load(file)
        except:
            self.aliases = {}

    def do(self, uri, hosts, isolate = False):
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
            'targets': {
                uri: {
                    'action': 'push',
                    'type': 'once',
                    'isolate': isolate
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
