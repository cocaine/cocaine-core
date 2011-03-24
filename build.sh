#!/bin/sh

g++ -c -Iinclude -pthread -o registry.o src/registry.cpp
g++ -c -Iinclude -pthread -o engines.o src/engines.cpp
g++ -c -Iinclude -pthread -o core.o src/core.cpp
g++ -c -Iinclude -pthread -o main.o src/main.cpp
g++ -ldl -lssl -lzmq -pthread -dynamic -o yappi registry.o engines.o core.o main.o

