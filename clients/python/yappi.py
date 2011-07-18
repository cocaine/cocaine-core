import zmq

class Client(object):
    def __init__(self, token, nodes):
        self.token = token
        self.context = zmq.Context()

        self.balancer = self.context.socket(zmq.REQ)
        self.nodes = {}
        self.sinks = []

        for node in nodes:
            if isinstance(node, tuple):
                endpoint, sink = node
            else:
                endpoint = node
                sink = None
                
            socket = self.context.socket(zmq.REQ)
            socket.connect(endpoint)
            self.balancer.connect(endpoint)
            self.nodes[endpoint] = socket

            if sink:
                socket = self.context.socket(zmq.SUB)
                socket.connect(sink)
                self.sinks.append(socket)

    def __construct(self, action, urls):
        if isinstance(urls, basestring):
            urls = [urls]
        
        request = {
            'version': 2,
            'token': self.token,
            'action': action,
            'targets': dict((url, {}) for url in urls)
        }

        return request

    def once(self, urls):
        self.balancer.send_json(self.__construct('once', urls))
        return self.balancer.recv_json()

    def map(self, urls):
        request = self.__construct('once', urls)
        [node.send_json(request) for node in self.nodes.itervalues()]
        return dict((name, node.recv_json()) for name, node in self.nodes.iteritems())

    def push(self, urls):
        raise NotImplementedError

    def drop(self, urls):
        raise NotImplementedError

    def cast(self, urls):
        raise NotImplementedError

    def dispell(self, urls):
        raise NotImplementedError

