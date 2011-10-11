#ifndef COCAINE_COMMON_HPP
#define COCAINE_COMMON_HPP

#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <tr1/cstdint>

#include <syslog.h>

#include <boost/version.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/tuple/tuple.hpp>

#include "cocaine/config.hpp"
#include "cocaine/helpers/unique_id.hpp"
#include "cocaine/helpers/birth_control.hpp"

#include "json/json.h"

#define EV_MINIMAL 0
#include <ev++.h>

// Driver types
#define AUTO        1
#define MANUAL      2
#define FILESYSTEM  3
#define SINK        4
#define SERVER      5

#endif
