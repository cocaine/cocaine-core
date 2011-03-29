import zmq

class Flow(object):
    def __init__(self, context, endpoint, key, fieldset = []):
        self.socket = context.socket(zmq.SUB)
        self.socket.setsockopt(zmq.HWM, 10)
        self.socket.connect(endpoint)
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
    def __init__(self, requests, export):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect(requests)
        self.export = export

    def subscribe(self, uri, timeout, ttl, fieldset = []):
        self.socket.send('loop %d %d %s' % (timeout, ttl, uri))
        result = self.socket.recv()
        
        if not result.startswith('e'):
            return Flow(self.context, self.export, result, fieldset)
        else:
            raise RuntimeError(result)

    def unsubscribe(self, flow):
        self.socket.send('unloop %s' % flow.key)
        result = self.socket.recv()
        if result == "ok":
            flow.socket.close()
        else:
            raise RuntimeError(result)
