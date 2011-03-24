#!/bin/sh

g++ -c -Iinclude -pthread -o registry.o src/registry.cpp
g++ -c -Iinclude -pthread -o engine.o src/engine.cpp
g++ -c -Iinclude -pthread -o core.o src/core.cpp
g++ -c -Iinclude -pthread -o polling_core.o src/polling_core.cpp
g++ -c -Iinclude -pthread -o main.o src/main.cpp
g++ -ldl -lssl -lzmq -pthread -dynamic -o yappi registry.o engine.o core.o polling_core.o main.o

