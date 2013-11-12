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

#ifndef COCAINE_IO_MESSAGES_HPP
#define COCAINE_IO_MESSAGES_HPP

#include "cocaine/common.hpp"

#include "cocaine/rpc/graph.hpp"
#include "cocaine/rpc/protocol.hpp"
#include "cocaine/rpc/tags.hpp"
#include "cocaine/rpc/types.hpp"

namespace cocaine { namespace io {

// Service presence control interface

namespace presence {

struct heartbeat {
    typedef presence_tag  tag;
    typedef recursive_tag transition_type;

    static
    const char*
    alias() {
        return "heartbeat";
    }

    typedef
     /* The UUID of the node, to detect the event of restart. */
        std::string
    result_type;
};

} // namespace presence

template<>
struct protocol<presence_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        presence::heartbeat
    > type;
};

// Service locator interface

namespace locator {

typedef std::tuple<std::string, uint16_t> endpoint_tuple_type;

struct resolve {
    typedef locator_tag tag;

    static
    const char*
    alias() {
        return "resolve";
    }

    typedef boost::mpl::list<
     /* An alias of the service to resolve. */
        std::string
    > tuple_type;

    typedef boost::mpl::list<
     /* An endpoint for the client to connect to in order to use the the service. */
        endpoint_tuple_type,
     /* Service protocol version. If the client wishes to use the service, the protocol
        versions must match. */
        unsigned int,
     /* A mapping between method slot numbers, method names and state protocol transitions for use
        in dynamic languages like Python or Ruby. */
        dispatch_graph_t
    > result_type;
};

struct synchronize {
    typedef locator_tag tag;

    static
    const char*
    alias() {
        return "synchronize";
    }

    typedef
     /* A full dump of all available services on this node. Used by metalocator to aggregate
        node information from the cluster. */
        std::map<std::string, tuple::fold<resolve::result_type>::type>
    result_type;
};

struct reports {
    typedef locator_tag tag;

    static
    const char*
    alias() {
        return "reports";
    }

    typedef std::map<
     /* Maps client remote endpoint to the number of streams and memory usage. */
        endpoint_tuple_type, std::tuple<size_t, size_t>
    > usage_report_type;

    typedef
     /* Service I/O usage counters: number of concurrent sessions and memory footprints. */
        std::map<std::string, std::tuple<size_t, usage_report_type>>
    result_type;
};

struct refresh {
    typedef locator_tag tag;

    static
    const char*
    alias() {
        return "refresh";
    }

    typedef boost::mpl::list<
     /* Name of the group to refresh. */
        std::string
    > tuple_type;
};

} // namespace locator

template<>
struct protocol<locator_tag> {
    typedef boost::mpl::int_<
        2
    >::type version;

    typedef boost::mpl::list<
        locator::resolve,
        locator::synchronize,
        locator::reports,
        locator::refresh
    > type;
};

// Old-school interface

namespace rpc {

struct handshake {
    typedef rpc_tag tag;

    typedef boost::mpl::list<
        /* peer id */ std::string
    > tuple_type;
};

struct heartbeat {
    typedef rpc_tag tag;
};

struct terminate {
    typedef rpc_tag tag;

    enum code: int {
        normal = 1,
        abnormal
    };

    typedef boost::mpl::list<
        /* code */   code,
        /* reason */ std::string
    > tuple_type;
};

struct invoke {
    typedef rpc_tag tag;

    typedef boost::mpl::list<
        /* event */ std::string
    > tuple_type;
};

struct chunk {
    typedef rpc_tag tag;

    typedef boost::mpl::list<
        /* chunk */ std::string
    > tuple_type;
};

struct error {
    typedef rpc_tag tag;

    typedef boost::mpl::list<
        /* code */   int,
        /* reason */ std::string
    > tuple_type;
};

struct choke {
    typedef rpc_tag tag;
};

} // namespace rpc

template<>
struct protocol<rpc_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        rpc::handshake,
        rpc::heartbeat,
        rpc::terminate,
        rpc::invoke,
        rpc::chunk,
        rpc::error,
        rpc::choke
    >::type type;
};

// Streaming interface

namespace streaming {

struct write {
    typedef streaming_tag tag;

    // Specifies that this slot doesn't switch the protocol dispatch.
    typedef recursive_tag transition_type;

    static
    const char*
    alias() {
        return "write";
    }

    typedef boost::mpl::list<
     /* Some chunk of data to be sent to the app. By convention, it is expected to be serialized
        with MessagePack, but obviously you can send whatever you want. */
        std::string
    > tuple_type;
};

struct close {
    typedef streaming_tag tag;

    static
    const char*
    alias() {
        return "close";
    }
};

} // namespace streaming

template<>
struct protocol<streaming_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        streaming::write,
        streaming::close
    >::type type;
};

// App invocation service interface

namespace app {

struct enqueue {
    typedef app_tag tag;

    // Allow clients to stream data into the apps.
    typedef streaming_tag transition_type;

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
        raw_t
    result_type;
};

struct info {
    typedef app_tag tag;

    static
    const char*
    alias() {
        return "info";
    }
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

// Logging service interface

namespace logging {

using cocaine::logging::priorities;

struct emit {
    typedef logging_tag tag;

    static
    const char*
    alias() {
        return "emit";
    }

    typedef boost::mpl::list<
     /* Log level for this message. Generally, you are not supposed to send messages with log
        levels higher than the current verbosity. */
        priorities,
     /* Message source. Messages originating from the user code should be tagged with
        'app/<name>' so that they could be routed separately. */
        std::string,
     /* Log message. Some meaningful string, with no explicit limits on its length, although
        underlying loggers might silently truncate it. */
        std::string
    > tuple_type;
};

struct verbosity {
    typedef logging_tag tag;

    static
    const char*
    alias() {
        return "verbosity";
    }

    typedef
     /* The current verbosity level of the of the core logging sink. */
        priorities
    result_type;
};

} // namespace logging

template<>
struct protocol<logging_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        logging::emit,
        logging::verbosity
    > type;
};

// Storage service interface

namespace storage {

struct read {
    typedef storage_tag tag;

    static
    const char*
    alias() {
        return "read";
    }

    typedef boost::mpl::list<
     /* Key namespace. Currently no ACL checks are performed, so in theory any app can read
        any other app data without restrictions. */
        std::string,
     /* Key. */
        std::string
    > tuple_type;

    typedef
     /* The stored value. Typically it will be serialized with msgpack, but it's not a strict
        requirement. But as there's no way to know the format, try to unpack it anyway. */
        std::string
    result_type;
};

struct write {
    typedef storage_tag tag;

    static
    const char*
    alias() {
        return "write";
    }

    typedef boost::mpl::list<
     /* Key namespace. */
        std::string,
     /* Key. */
        std::string,
     /* Value. Typically, it should be serialized with msgpack, so that the future reader could
        assume that it can be deserialized safely. */
        std::string,
     /* Tag list. Imagine these are your indexes. */
        optional<std::vector<std::string>>
    > tuple_type;
};

struct remove {
    typedef storage_tag tag;

    static
    const char*
    alias() {
        return "remove";
    }

    typedef boost::mpl::list<
     /* Key namespace. Again, due to the lack of ACL checks, any app can obliterate the whole
        storage for all the apps in the cluster. Beware. */
        std::string,
     /* Key. */
        std::string
    > tuple_type;
};

struct find {
    typedef storage_tag tag;

    static
    const char*
    alias() {
        return "find";
    }

    typedef boost::mpl::list<
     /* Key namespace. A good start point to find all the keys to remove to render the system
        useless! Well, one day we'll implement ACLs. */
        std::string,
     /* Tag list. This is actually your query. */
        std::vector<std::string>
    > tuple_type;

    typedef
     /* A list of all the keys in the given key namespace. */
        std::vector<std::string>
    result_type;
};

} // namespace storage

template<>
struct protocol<storage_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        storage::read,
        storage::write,
        storage::remove,
        storage::find
    > type;
};

// Node service interface

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
};

struct list {
    typedef node_tag tag;

    static
    const char*
    alias() {
        return "list";
    }
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
