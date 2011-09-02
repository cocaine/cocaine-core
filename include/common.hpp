#ifndef YAPPI_COMMON_HPP
#define YAPPI_COMMON_HPP

#include <stdint.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <syslog.h>
#include <time.h>

#define EV_MINIMAL 0
#include <ev++.h>

#include <zmq.hpp>

#if ZMQ_VERSION < 20107
    #error ZeroMQ version 2.1.7+ required!
#endif

#include <boost/version.hpp>

#if BOOST_VERSION >= 103500
    #define HISTORY_ENABLED
#endif

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/ptr_container/ptr_map.hpp>

#include "config.hpp"
#include "auto_uuid.hpp"
#include "birth_control.hpp"

#include "json/json.h"

#endif
