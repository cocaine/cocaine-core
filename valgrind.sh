#!/bin/bash

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo . || exit 1

make

valgrind --tool=memcheck --leak-check=full --show-reachable=yes \
    ./cocained tcp://*:10000 --instance valgrind


