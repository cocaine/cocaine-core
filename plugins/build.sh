#!/bin/sh

g++ -o sysinfo.o -c -O2 -Wall -pedantic -pthread -I../include sysinfo/sysinfo.cpp
g++ -o sysinfo.so -shared -pthread sysinfo.o

g++ -o mysql.o -c -O2 -Wall -pedantic -pthread -I../include mysql/mysql.cpp
g++ -o mysql.so -shared -lmysqlclient -pthread mysql.o
