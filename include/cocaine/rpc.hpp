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

#ifndef COCAINE_RPC_HPP
#define COCAINE_RPC_HPP

#include "cocaine/io.hpp"

namespace cocaine { 

namespace tags {
    struct rpc_tag;
    struct control_tag;
}

namespace rpc {
    struct ping {
        typedef tags::rpc_tag tag;
    };

    struct pong {
        typedef tags::rpc_tag tag;
    };

    struct suicide {
        typedef tags::rpc_tag tag;

        enum reasons: int {
            normal,
            abnormal
        };

        typedef boost::mpl::list<
            int,
            std::string
        > tuple_type;
    };

    struct terminate {
        typedef tags::rpc_tag tag;
    };

    struct invoke {
        typedef tags::rpc_tag tag;
        
        typedef boost::mpl::list<
            unique_id_t,
            std::string
        > tuple_type;
    };

    struct chunk {
        typedef tags::rpc_tag tag;
        
        typedef boost::mpl::list<
            unique_id_t,
            std::string
        > tuple_type;
    };

    struct error {
        typedef tags::rpc_tag tag;
        
        typedef boost::mpl::list<
            unique_id_t,
            int,
            std::string
        > tuple_type;
    };

    struct choke {
        typedef tags::rpc_tag tag;

        typedef boost::mpl::list<
            unique_id_t
        > tuple_type;
    };
}

namespace control {
    struct status {
        typedef tags::control_tag tag;
    };

    struct terminate {
        typedef tags::control_tag tag;
    };
}

namespace io {
    template<>
    struct dispatch<tags::rpc_tag> {
        typedef boost::mpl::list<
            rpc::ping,
            rpc::pong,
            rpc::suicide,
            rpc::terminate,
            rpc::invoke,
            rpc::chunk,
            rpc::error,
            rpc::choke
        >::type category;
    };

    template<>
    struct dispatch<tags::control_tag> {
        typedef boost::mpl::list<
            control::status,
            control::terminate
        >::type category;
    };
}

} // namespace cocaine

#endif
