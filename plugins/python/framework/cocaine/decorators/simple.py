# encoding: utf-8

import types
import io
import msgpack
import json


class SimpleTimer(object):
    def __init__(self, processor = None):
        if processor is not None:
            assert(callable(processor))
            self.process = processor

    def __call__(self):
        try:
            self.result = self.process()
        except AttributeError:
            raise NotImplementedError("You have to implement the process() method")

        if isinstance(self.result, types.DictType):
            return [json.dumps(self.result)]
        else:
            try:
                return [json.dumps({"result": self.result})]
            except TypeError:
                self.result = iter(self.result)
                return self

    def __iter__(self):
        chunk = next(self.result)
        
        if isinstance(chunk, types.DictType):    
            yield json.dumps(chunk)
        else:
            yield json.dumps({"result": chunk})


class SimpleServer(object):
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

