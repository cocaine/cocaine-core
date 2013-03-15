/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/rpc/protocol.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/json.hpp"
#include "cocaine/unique_id.hpp"

namespace cocaine { namespace io {

namespace tags {
    struct rpc_tag;
    struct control_tag;
}

namespace rpc {
    struct handshake {
        typedef tags::rpc_tag tag;

        typedef boost::mpl::list<
            /* client id */ unique_id_t
        > tuple_type;
    };

    struct heartbeat {
        typedef tags::rpc_tag tag;
    };

    struct terminate {
        typedef tags::rpc_tag tag;

        enum codes: int {
            normal,
            abnormal
        };

        typedef boost::mpl::list<
            /* reason */  int,
            /* message */ std::string
        > tuple_type;
    };

    struct invoke {
        typedef tags::rpc_tag tag;

        typedef boost::mpl::list<
            /* session */ uint64_t,
            /* event */   api::event_t
        > tuple_type;
    };

    struct chunk {
        typedef tags::rpc_tag tag;

        typedef boost::mpl::list<
            /* session */ uint64_t,
            /* data */    std::string
        > tuple_type;
    };

    struct error {
        typedef tags::rpc_tag tag;

        typedef boost::mpl::list<
            /* session */ uint64_t,
            /* code */    int,
            /* message */ std::string
        > tuple_type;
    };

    struct choke {
        typedef tags::rpc_tag tag;

        typedef boost::mpl::list<
            /* session */ uint64_t
        > tuple_type;
    };
}

namespace control {
    struct report {
        typedef tags::control_tag tag;
    };

    struct info {
        typedef tags::control_tag tag;

        typedef boost::mpl::list<
            /* info */ Json::Value
        > tuple_type;
    };

    struct terminate {
        typedef tags::control_tag tag;
    };
}

template<>
struct protocol<tags::rpc_tag> {
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

template<>
struct protocol<tags::control_tag> {
    typedef boost::mpl::list<
        control::report,
        control::info,
        control::terminate
    >::type type;
};

}} // namespace cocaine::io

#endif
