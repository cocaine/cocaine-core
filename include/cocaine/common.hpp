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

using cocaine::helpers::unique_id_t;
using cocaine::helpers::birth_control_t;

#include "json/json.h"

#define EV_MINIMAL 0
#include <ev++.h>

#endif
