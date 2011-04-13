import zmq

class Flow(object):
    def __init__(self, context, endpoint, uri, timeout, key, fieldset = []):
        self.socket = context.socket(zmq.SUB)
        self.socket.setsockopt(zmq.HWM, 10)
        self.socket.connect(endpoint)
        self.uri = uri
        self.timeout = timeout
        self.key = key

        if fieldset:
            for field in fieldset:
                self.socket.setsockopt(zmq.SUBSCRIBE,
                    "%s %s" % (key, field))
        else:
            self.socket.setsockopt(zmq.SUBSCRIBE, key)

    def __iter__(self):
        return self

    def next(self):
        try:
            envelope, data = self.socket.recv_multipart(zmq.NOBLOCK)
        except zmq.ZMQError, e:
            raise StopIteration

        key, field, timestamp = envelope.split(' ')

        return (field, data)

class Client(object):
    def __init__(self, name, requests, export):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.setsockopt(zmq.IDENTITY, name)
        self.socket.connect(requests)
        self.export = export

    def subscribe(self, uri, timeout, fieldset = []):
        self.socket.send('start %d %s' % (timeout, uri))
        key = self.socket.recv()
        
        if not key.startswith('e'):
            return Flow(self.context, self.export, uri, timeout, key, fieldset)
        else:
            raise RuntimeError(key)

    def unsubscribe(self, flow):
        self.socket.send('stop %d %s' % (flow.timeout, flow.uri))
        result = self.socket.recv()

        if result == "success":
            flow.socket.close()
        else:
            raise RuntimeError(result)
