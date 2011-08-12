#ifndef YAPPI_COMMON_HPP
#define YAPPI_COMMON_HPP

#include <stdint.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <syslog.h>
#include <time.h>

#include <ev++.h>

#include <zmq.hpp>

#if ZMQ_VERSION < 20107
    #error ZeroMQ version 2.1.7+ required!
#endif

#include <boost/noncopyable.hpp>

#include "auto_uuid.hpp"
#include "digest.hpp"
#include "sockets.hpp"
#include "birth_control.hpp"

#endif
