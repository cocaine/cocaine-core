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

// Streaming interface template

template<class T>
struct streaming {

struct write {
    typedef streaming_tag<T> tag;

    // Specifies that this slot doesn't switch the protocol dispatch.
    typedef recursive_tag transition_type;

    static
    const char*
    alias() {
        return "write";
    }

    typedef boost::mpl::list<
     /* Some chunk of data to be sent to the service. */
        T
    > tuple_type;
};

struct error {
    typedef streaming_tag<T> tag;

    static
    const char*
    alias() {
        return "error";
    }

    typedef boost::mpl::list<
     /* Error code. */
        int,
     /* Human-readable error description. */
        std::string
    > tuple_type;
};

struct close {
    typedef streaming_tag<T> tag;

    static
    const char*
    alias() {
        return "close";
    }
};

}; // struct streaming

template<class T>
struct protocol<streaming_tag<T>> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef typename boost::mpl::list<
        typename streaming<T>::write,
        typename streaming<T>::error,
        typename streaming<T>::close
    >::type type;
};

}} // namespace cocaine::io

#endif
