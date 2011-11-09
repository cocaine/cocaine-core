# coding: utf-8

import os

class Tail(object):
    def __init__(self, filename):
        self.filename = filename
        self.inode = 0

    def __iter__(self):
        file = open(self.filename, 'r')
        inode = os.fstat(file.fileno()).st_ino
        
        if(self.inode != inode):
            self.position = 0
            self.inode = inode

        file.seek(self.position - 1 if self.position > 0 else 0)
        result = self.parse(line for line in file)
        self.position = file.tell()

        yield result

    def parse(self, lines):
        raise NotImplementedError("You have to provide the parser implementation")
