#!/bin/bash

scons || exit 1

LD_LIBRARY_PATH=build valgrind --tool=memcheck --leak-check=full --track-origins=yes --show-reachable=yes build/yappi tcp://*:5100 --export tcp://*:5101 --watermark 1000
