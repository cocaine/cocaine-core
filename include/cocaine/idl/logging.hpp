/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_LOGGING_SERVICE_INTERFACE_HPP
#define COCAINE_LOGGING_SERVICE_INTERFACE_HPP

#include "cocaine/rpc/protocol.hpp"

#include <blackhole/attribute.hpp>

namespace cocaine { namespace io {

// Logging service interface

struct log_tag;

struct log {

struct emit {
    typedef log_tag tag;

    static
    const char*
    alias() {
        return "emit";
    }

    typedef boost::mpl::list<
     /* Log level for this message. Generally, you are not supposed to send messages with log
        levels higher than the current verbosity. */
        logging::priorities,
     /* Message source. Messages originating from the user code should be tagged with
        'app/<name>' so that they could be routed separately. */
        std::string,
     /* Log message. Some meaningful string, with no explicit limits on its length, although
        underlying loggers might silently truncate it. */
        std::string,
     /* Log event attached attributes. */
        optional<blackhole::log::attributes_t>
    > tuple_type;
};

struct verbosity {
    typedef log_tag tag;

    static
    const char*
    alias() {
        return "verbosity";
    }

    typedef stream_of<
     /* The current verbosity level of the of the core logging sink. */
        logging::priorities
    >::tag drain_type;
};

struct set_verbosity {
    typedef log_tag tag;

    static
    const char*
    alias() {
        return "set_verbosity";
    }

    typedef boost::mpl::list<
     /* Proposed verbosity level. */
        logging::priorities
    > tuple_type;
};

}; // struct log

template<>
struct protocol<log_tag> {
    typedef boost::mpl::int_<
        2
    >::type version;

    typedef boost::mpl::list<
        log::emit,
        log::verbosity,
        log::set_verbosity
    > messages;

    typedef log scope;
};

}} // namespace cocaine::io

#endif
