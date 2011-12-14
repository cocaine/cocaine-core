# encoding: utf-8

import io
import msgpack

class NativeServer(object):
    def __init__(self, processor = None):
        if processor is not None:
            assert(callable(processor))
            self.process = processor

    def __call__(self, request):
        request = msgpack.unpack(io.BytesIO(request))

        try:
            self.result = self.process(request)
        except AttributeError:
            raise NotImplementedError("You have to implement the process() method")

        try:
            return [msgpack.packs(self.result)]
        except TypeError:
            self.result = iter(self.result)
            return self

    def __iter__(self):
        while True:
            yield msgpack.packs(next(self.result)) 

