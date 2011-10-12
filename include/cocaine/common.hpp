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

// Message types
#define PUSH      1 /* engine pushes a task to an overseer */
#define DROP      2 /* engine drops a task from an overseer */
#define TERMINATE 3 /* engine terminates an overseer */
#define FUTURE    4 /* overseer fulfills an engine's request */
#define SUICIDE   5 /* overseer performs a suicide */
#define EVENT     6 /* driver sends the invocation results to the core */
#define HEARTBEAT 7 /* overseer is reporting that it's still alive */

// Driver types
#define AUTO        1
#define MANUAL      2
#define FILESYSTEM  3
#define SINK        4
#define SERVER      5

#endif
