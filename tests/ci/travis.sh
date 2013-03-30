#!/bin/sh -ex

# Travis-CI hacks
yes | sudo add-apt-repository ppa:yandex-opensource/cocaine-dev
sudo apt-get update -qq

# Get rid of the broken Travis ZeroMQ package
yes | sudo apt-get remove libzmq3

# Install build-depends
yes | sudo mk-build-deps -i

# Build packages
yes | debuild -e CC -e CXX --prepend-path="/usr/local/bin/" -uc -us

# TODO(kobolog@): Install depends

# Install packages
# sudo -- dpkg -i ../*.deb

# TODO(kobolog@): Run stress test
