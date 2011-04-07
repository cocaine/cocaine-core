#!/bin/bash

scons || exit 1

LD_LIBRARY_PATH=build valgrind --tool=memcheck --leak-check=full --track-origins=yes --show-reachable=yes build/yappi -l tcp://*:1710 -e tcp://*:1711 -p build/plugins
