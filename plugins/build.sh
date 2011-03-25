#!/bin/sh

g++ -o sysinfo.o -g -c -O2 -Wall -pedantic -pthread -I../include sysinfo/sysinfo.cpp
g++ -o sysinfo.so -shared -pthread sysinfo.o

g++ -o mysql.o -g -c -O2 -Wall -pedantic -pthread -I../include mysql/mysql.cpp
g++ -o mysql.so -shared -lmysqlclient -pthread mysql.o

g++ -o python.o -g -c -O2 -Wall -pedantic -pthread -I../include -I/usr/include/python2.6 python/python.cpp
g++ -o python.so -shared -lpython2.6 -pthread python.o
