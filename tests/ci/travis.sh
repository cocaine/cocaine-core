#!/bin/sh -ex

# Travis-CI hacks
yes | sudo add-apt-repository ppa:yandex-opensource/cocaine-dev
sudo apt-get update -qq

# Fix the initial package list
yes | sudo apt-get remove libzmq3

# Install build-depends
yes | sudo mk-build-deps -i

# Link the broken ZeroMQ library to a right place
sudo ln -s /usr/lib/x86_64-linux-gnu/libzmq.so /usr/lib/libzmq.so

# Build packages
yes | debuild -e CC -e CXX --prepend-path="/usr/local/bin/" -uc -us

# TODO(kobolog@): Install depends

# Install packages
# sudo -- dpkg -i ../*.deb

# TODO(kobolog@): Run stress test
