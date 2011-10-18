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
#define AUTO        1   /* do something every n milliseconds */
#define CRON        2   /* do something based on a cron-like schedule */
#define MANUAL      3   /* do something when application says */
#define FILESYSTEM  4   /* do something when there's a change on the filesystem */
#define SINK        5   /* do something when there's a message on the socket */

// Message types
#define INVOKE      1   /* engine -> worker: do something*/
#define TERMINATE   2   /* engine -> worker: die */
#define FUTURE      3   /* worker -> engine: something is done, here are the results */
#define SUICIDE     4   /* worker -> engine: i am useless, kill me */
#define HEARTBEAT   5   /* worker -> engine: i am alive, don't kill me */

#endif
