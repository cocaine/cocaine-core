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

#include <string>

#include "cocaine/rpc/protocol.hpp"

namespace cocaine { namespace io {

struct worker_tag;

struct worker {

struct control_tag;

/// This is the only message, that the worker can sent to be able create control channel.
struct handshake {
    static const char* alias() {
        return "handshake";
    }

    typedef worker_tag tag;

    typedef control_tag dispatch_type;
    typedef control_tag upstream_type;

    typedef boost::mpl::list<
     /* The unique worker identifyer (usually uuid). */
        std::string
    >::type argument_type;
};

struct heartbeat {
    static const char* alias() {
        return "heartbeat";
    }

    typedef control_tag tag;
    typedef control_tag dispatch_type;
};

struct terminate {
    static const char* alias() {
        return "terminate";
    }

    typedef control_tag tag;
};

struct rpc_tag;

struct rpc {
    struct invoke {
        static const char* alias() {
            return "invoke";
        }

        typedef rpc_tag tag;

        typedef stream_of<std::string>::tag dispatch_type;
        typedef stream_of<std::string>::tag upstream_type;

        typedef boost::mpl::list<
         /* Event name. */
            std::string
        >::type argument_type;
    };
};

}; // struct worker

template<>
struct protocol<worker_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        worker::handshake
    >::type messages;

    typedef worker scope;
};

template<>
struct protocol<worker::control_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        worker::heartbeat,
        worker::terminate
    >::type messages;

    typedef worker scope;
};

template<>
struct protocol<worker::rpc_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        worker::rpc::invoke
    >::type messages;

    typedef worker::rpc scope;
};

}} // namespace cocaine::io

#endif
