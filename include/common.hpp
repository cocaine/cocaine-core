#ifndef YAPPI_COMMON_HPP
#define YAPPI_COMMON_HPP

#include <stdint.h>

#include <string>
#include <vector>
#include <map>
#include <deque>

#include <time.h>
#include <syslog.h>

#include <ev++.h>
#include <zmq.hpp>

#if ZMQ_VERSION < 20100
    #error ZeroMQ version 2.1.0+ required!
#endif


#endif
