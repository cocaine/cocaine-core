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

#ifndef COCAINE_NODE_SERVICE_INTERFACE_HPP
#define COCAINE_NODE_SERVICE_INTERFACE_HPP

#include "cocaine/idl/streaming.hpp"

#include "cocaine/rpc/protocol.hpp"
#include "cocaine/rpc/tags.hpp"

namespace cocaine { namespace io {

// App invocation service interface

struct app_tag;

namespace app {

struct enqueue {
    typedef app_tag tag;

    // Allow clients to stream data into the apps.
    typedef streaming_tag<std::string> transition_type;

    static
    const char*
    alias() {
        return "enqueue";
    }

    typedef boost::mpl::list<
     /* Event name. This name is intentionally dynamic so that the underlying application can
        do whatever it wants using these event names, for example handle every possible one. */
        std::string,
     /* Tag. Event can be enqueued to a specific worker with some user-defined name. */
        optional<std::string>
    > tuple_type;

    typedef
     /* Some other arbitrary sequence of bytes, streamed back to the client in chunks. */
        std::string
    result_type;
};

struct info {
    typedef app_tag tag;

    static
    const char*
    alias() {
        return "info";
    }

    typedef
     /* Various runtime information about the running app. */
        dynamic_t
    result_type;
};

} // namespace app

template<>
struct protocol<app_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        app::enqueue,
        app::info
    > type;
};

// Node service interface

struct node_tag;

namespace node {

struct start_app {
    typedef node_tag tag;

    static
    const char*
    alias() {
        return "start_app";
    }

    typedef boost::mpl::list<
     /* Runlist. A mapping between app names and profile names. Errors are reported on a
        per-app basis. */
        std::map<std::string, std::string>
    > tuple_type;

    typedef
     /* Operation outcome. */
        dynamic_t
    result_type;
};

struct pause_app {
    typedef node_tag tag;

    static
    const char*
    alias() {
        return "pause_app";
    }

    typedef boost::mpl::list<
     /* A list of app names to suspend. Errors are reported on a per-app basis, as well. */
        std::vector<std::string>
    > tuple_type;

    typedef
     /* Operation outcome. */
        dynamic_t
    result_type;
};

struct list {
    typedef node_tag tag;

    static
    const char*
    alias() {
        return "list";
    }

    typedef
     /* A list of running app names. */
        dynamic_t
    result_type;
};

} // namespace node

template<>
struct protocol<node_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        node::start_app,
        node::pause_app,
        node::list
    > type;
};

}} // namespace cocaine::io

#endif
