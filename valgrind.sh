#!/bin/bash

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo . || exit 1

LD_LIBRARY_PATH=build valgrind --tool=memcheck --leak-check=full --show-reachable=yes \
    ./cocained tcp://*:45100 --export tcp://*:45101 --watermark 1000 \
    --storage-driver files --storage-location build/tmp \
    --instance valgrind
