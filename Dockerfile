FROM ubuntu:trusty

# Pass build branch from travis
ARG BUILD_BRANCH
ENV BUILD_BRANCH=$BUILD_BRANCH

RUN apt-get -y -qq update
RUN apt-get -y -qq install build-essential devscripts equivs git-core

# Yep. It's a bit hackish to install our internal dependencies through git.
## Build and install Metrics.
RUN git clone https://github.com/3Hren/metrics --recursive -b master /build/metrics
RUN cd /build/metrics && \
    DEBIAN_FRONTEND=noninteractive mk-build-deps -ir -t "apt-get -qq --no-install-recommends"
RUN cd /build/metrics && \
    yes | debuild -e CC -e CXX -uc -us -j$(cat /proc/cpuinfo | fgrep -c processor) && \
    debi

## Build and install Blackhole.
RUN git clone https://github.com/3Hren/blackhole --recursive -b master /build/blackhole
RUN cd /build/blackhole && \
    DEBIAN_FRONTEND=noninteractive mk-build-deps -ir -t "apt-get -qq --no-install-recommends"
RUN cd /build/blackhole && \
    yes | debuild -e CC -e CXX -uc -us -j$(cat /proc/cpuinfo | fgrep -c processor) && \
    debi

# Hack to cache plugins dependencies.
RUN apt-get -qq install libarchive-dev uuid-dev libcgroup-dev libboost-filesystem-dev  libboost-thread-dev libnl-3-dev libnl-genl-3-dev libzookeeper-mt-dev libpqxx-dev

# Build and install cocaine-core.
COPY . /build/cocaine-core
RUN cd /build/cocaine-core && \
    DEBIAN_FRONTEND=noninteractive mk-build-deps -ir -t "apt-get -qq --no-install-recommends"
RUN cd /build/cocaine-core && \
    yes | debuild -e CC -e CXX -uc -us -j$(cat /proc/cpuinfo | fgrep -c processor) && \
    debi

# Build and install cocaine-plugins.
RUN git clone https://github.com/3Hren/cocaine-plugins --recursive -b $BUILD_BRANCH /build/cocaine-plugins
RUN cd /build/cocaine-plugins && \
    cmake -DELASTICSEARCH=OFF -DMONGO=OFF -DURLFETCH=OFF -DDOCKER=OFF -DELLIPTICS=OFF . && \
    make

# Cleanup.
RUN apt-get -y -qq purge cocaine-core-build-deps && \
    apt-get -y -qq purge blackhole-build-deps && \
    apt-get -y -qq purge metrics-build-deps && \
    apt-get -y -qq purge build-essential devscripts equivs git-core && \
    apt-get -y -qq autoremove --purge
