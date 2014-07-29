/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_SERVICE_LOCATOR_INTERFACE_HPP
#define COCAINE_SERVICE_LOCATOR_INTERFACE_HPP

#include "cocaine/rpc/graph.hpp"
#include "cocaine/rpc/protocol.hpp"

#include "cocaine/tuple.hpp"

namespace cocaine { namespace io {

// Service locator interface

struct locator_tag;

struct locator {

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

    typedef std::tuple<
     /* Fully-qualified domain name of the service node. */
        std::string,
     /* Service port in host byte order. */
        uint16_t
    > endpoint_tuple_type;

    typedef boost::mpl::list<
     /* An endpoint for the client to connect to in order to use the the service. */
        endpoint_tuple_type,
     /* Service protocol version. If the client wishes to use the service, the protocol
        versions must match. */
        unsigned int,
     /* A mapping between method slot numbers, method names and state protocol transitions for use
        in dynamic languages like Python or Ruby. */
        dispatch_graph_t
    > value_type;

    typedef streaming_tag<value_type> upstream_type;
};

struct synchronize {
    typedef locator_tag tag;

    static
    const char*
    alias() {
        return "synchronize";
    }

    typedef stream_of<
     /* A full dump of all available services on this node. Used by metalocator to aggregate
        node information from the cluster. */
        std::map<std::string, tuple::fold<resolve::value_type>::type>
    >::tag upstream_type;
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

}; // struct locator

template<>
struct protocol<locator_tag> {
    typedef boost::mpl::int_<
        2
    >::type version;

    typedef boost::mpl::list<
        locator::resolve,
        locator::synchronize,
        locator::refresh
    > messages;

    typedef locator scope;
};

}} // namespace cocaine::io

#endif
