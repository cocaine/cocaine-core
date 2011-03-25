#!/bin/bash

scons || exit 1

LD_LIBRARY_PATH=lib valgrind --tool=memcheck --leak-check=full --track-origins=yes --show-reachable=yes bin/yappi -l tcp://*:1710 -e tcp://*:1711 -p plugins &
PID=$!

tailf /var/log/syslog | grep yappi

kill -SIGINT $PID
