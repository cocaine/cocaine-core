#ifndef YAPPI_COMMON_HPP
#define YAPPI_COMMON_HPP

#include <stdint.h>

#include <string>
#include <vector>
#include <map>

#include <time.h>
#include <syslog.h>

#define EV_USE_MONOTONIC 1
#define EV_USE_NANOSLEEP 1
#include <ev++.h>

#include <zmq.hpp>

#if ZMQ_VERSION < 20100
    #error ZeroMQ version 2.1.0+ required!
#endif

#include <boost/noncopyable.hpp>

#include "id.hpp"
#include "sockets.hpp"

#endif
