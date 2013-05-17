#!/bin/sh -ex

# Install build-depends
yes | sudo mk-build-deps -i

# Build packages
yes | debuild -e CC -e CXX --prepend-path="/usr/local/bin/" -uc -us

# TODO: Install packages
# TODO: Run tests
