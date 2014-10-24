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

#include "cocaine/idl/primitive.hpp"

#include "cocaine/rpc/graph.hpp"
#include "cocaine/rpc/protocol.hpp"

#include "cocaine/tuple.hpp"

#include <boost/asio/ip/tcp.hpp>

namespace cocaine { namespace io {

struct locator_tag;

// Service locator interface

struct locator {

struct resolve {
    typedef locator_tag tag;

    static const char* alias() {
        return "resolve";
    }

    typedef boost::mpl::list<
     /* An alias of the service to resolve. */
        std::string,
     /* Routing seed. Can be used to consistently map users to service versions. */
        optional<std::string>
    > argument_type;

    typedef option_of<
     /* Endpoints for the client to connect to in order to use the the service. */
        std::vector<boost::asio::ip::tcp::endpoint>,
     /* Service protocol version. If the client wishes to use the service, the protocol
        versions must match. */
        unsigned int,
     /* A mapping between slot id numbers, message names and state transitions for both the message
        dispatch and upstream types to use in dynamic languages like Python, Ruby or JavaScript. */
        graph_basis_t
    >::tag upstream_type;
};

struct connect {
    typedef locator_tag tag;

    static const char* alias() {
        return "connect";
    }

    typedef boost::mpl::list<
     /* Node ID. */
        std::string,
     /* Snapshot generation ID. */
        optional<uint64_t>
    > argument_type;

    typedef stream_of<
     /* A full dump of all available services on this node. Used by metalocator to aggregate
        node information from the cluster. */
        std::map<std::string, tuple::fold<protocol<resolve::upstream_type>::sequence_type>::type>
    >::tag upstream_type;
};

struct refresh {
    typedef locator_tag tag;

    static const char* alias() {
        return "refresh";
    }

    typedef boost::mpl::list<
     /* Name of the group to refresh. */
        std::vector<std::string>
    > argument_type;
};

struct cluster {
    typedef locator_tag tag;

    static const char* alias() {
        return "cluster";
    }

    typedef option_of<
        std::map<std::string, boost::asio::ip::tcp::endpoint>
    >::tag upstream_type;
};

}; // struct locator

template<>
struct protocol<locator_tag> {
    typedef boost::mpl::int_<
        2
    >::type version;

    typedef boost::mpl::list<
        locator::resolve,
        locator::connect,
        locator::refresh,
        locator::cluster
    > messages;

    typedef locator scope;
};

}} // namespace cocaine::io

namespace cocaine { namespace error {

enum locator_errors {
    service_not_available = 1,
    routing_storage_error
};

auto
make_error_code(locator_errors code) -> boost::system::error_code;

}} // namespace cocaine::error

namespace boost { namespace system {

template<>
struct is_error_code_enum<cocaine::error::locator_errors>:
    public true_type
{ };

}} // namespace boost::system

#endif
