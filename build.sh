#!/bin/sh

g++ -c -Iinclude -pthread -o registry.o src/registry.cpp
g++ -c -Iinclude -pthread -o core.o src/core.cpp
g++ -c -Iinclude -pthread -o timed_core.o src/timed_core.cpp
g++ -c -Iinclude -pthread -o engine.o src/engine.cpp
g++ -c -Iinclude -pthread -o main.o src/main.cpp
g++ -ldl -lssl -lzmq -pthread -dynamic -o yappi engine.o core.o main.o timed_core.o registry.o

