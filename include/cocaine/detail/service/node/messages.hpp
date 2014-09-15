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

#ifndef COCAINE_ENGINE_INTERFACE_HPP
#define COCAINE_ENGINE_INTERFACE_HPP

#include "cocaine/rpc/protocol.hpp"

namespace cocaine { namespace io {

struct rpc_tag;

struct rpc {

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

}; // struct rpc

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
    > messages;

    typedef rpc scope;
};

struct control_tag;

struct control {

struct report {
    typedef control_tag tag;
};

struct info {
    typedef control_tag tag;

    typedef boost::mpl::list<
        /* info */ dynamic_t
    > tuple_type;
};

struct terminate {
    typedef control_tag tag;
};

}; // struct control

template<>
struct protocol<control_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        control::report,
        control::info,
        control::terminate
    > messages;

    typedef control scope;
};

}} // namespace cocaine::io

#endif
