/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_CONTEXT_SIGNALS_INTERFACE_HPP
#define COCAINE_CONTEXT_SIGNALS_INTERFACE_HPP

#include "cocaine/idl/locator.hpp"

namespace cocaine { namespace io {

struct context_tag;

// Context signals interface

struct context {

struct prepared {
    typedef context_tag tag;
    typedef context_tag dispatch_type;

    static const char* alias() {
        return "prepared";
    }

    typedef void upstream_type;
};

struct shutdown {
    typedef context_tag tag;
    typedef context_tag dispatch_type;

    static const char* alias() {
        return "shutdown";
    }

    typedef void upstream_type;
};

struct service {
    struct exposed {
        typedef context_tag tag;
        typedef context_tag dispatch_type;

        static const char* alias() {
            return "exposed";
        }

        typedef boost::mpl::list<
            std::string,
            std::tuple<std::vector<asio::ip::tcp::endpoint>, unsigned int, graph_root_t>
        >::type argument_type;

        typedef void upstream_type;
    };

    struct removed {
        typedef context_tag tag;
        typedef context_tag dispatch_type;

        static const char* alias() {
            return "removed";
        }

        typedef boost::mpl::list<
            std::string,
            std::tuple<std::vector<asio::ip::tcp::endpoint>, unsigned int, graph_root_t>
        >::type argument_type;

        typedef void upstream_type;
    };
};

}; // struct context

template<>
struct protocol<context_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        // Fired after context bootstrap. Means that all essential services are now running.
        context::prepared,
        // Fired first thing on context shutdown. This is a very good time to cleanup persistent
        // connections, synchronize disk state and so on.
        context::shutdown,
        // Fired on service creation, after service's thread is launched and is ready to accept
        // and process new incoming connections.
        context::service::exposed,
        // Fired on service destruction, after the service was removed from its endpoints, but
        // before the service object is actually destroyed.
        context::service::removed
    >::type messages;

    typedef context scope;
};

}} // namespace cocaine::io

#endif
