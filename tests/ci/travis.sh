#!/bin/sh -ex

# Travis-CI hacks
sudo apt-get remove -y --purge zeromq
yes | sudo add-apt-repository ppa:yandex-opensource/cocaine-dev
sudo apt-get update -qq

# Install build-depends
yes | sudo mk-build-deps -i

# Build packages
yes | debuild -uc -us

# Install packages
sudo dpkg -i ../*.deb

# Run stress test
# TODO(kobolog@):
