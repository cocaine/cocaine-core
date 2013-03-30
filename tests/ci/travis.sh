#!/bin/sh -ex

# Travis-CI hacks
yes | sudo add-apt-repository ppa:yandex-opensource/cocaine-dev
sudo apt-get update -qq

# Fix the initial package list
yes | sudo apt-get remove libzmq3
yes | sudo apt-get install wget

# Install build-depends
yes | sudo mk-build-deps -i

# Link the broken ZeroMQ library to a right place
ln -s /usr/lib/x86_64-linux-gnu/libzmq.so /usr/lib/libzmq.so

# Fetch the now-gone ZeroMQ C++ bindings
wget -O /usr/include/zmq.hpp https://raw.github.com/zeromq/cppzmq/master/zmq.hpp

# Build packages
yes | debuild -e CC -e CXX --prepend-path="/usr/local/bin/" -uc -us

# TODO(kobolog@): Install depends

# Install packages
# sudo -- dpkg -i ../*.deb

# TODO(kobolog@): Run stress test
