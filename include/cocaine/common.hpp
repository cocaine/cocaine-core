#ifndef COCAINE_COMMON_HPP
#define COCAINE_COMMON_HPP

#include <map>
#include <stdexcept>
#include <string>
#include <tr1/cstdint>
#include <vector>

#include <syslog.h>

#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_unordered_map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/version.hpp>

#define EV_MINIMAL 0
#include <ev++.h>

#include <zmq.hpp>

#if ZMQ_VERSION < 20107
    #error ZeroMQ version 2.1.7+ required!
#endif

#include "cocaine/config.hpp"
#include "cocaine/helpers/birth_control.hpp"
#include "cocaine/helpers/json.hpp"
#include "cocaine/helpers/unique_id.hpp"

using cocaine::helpers::unique_id_t;
using cocaine::helpers::birth_control_t;

#endif
