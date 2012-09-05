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

namespace rpc {
    struct core_tag;

    struct heartbeat {
        typedef core_tag tag;
    };

    struct terminate {
        typedef core_tag tag;
    };

    struct invoke {
        typedef core_tag tag;
        
        typedef boost::tuple<
            const std::string&,
            zmq::message_t&
        > tuple_type;
    };

    struct chunk {
        typedef core_tag tag;
        
        typedef boost::tuple<
            zmq::message_t&
        > tuple_type;
    };

    struct error {
        typedef core_tag tag;
        
        typedef boost::tuple<
            int,
            std::string
        > tuple_type;
    };

    struct choke {
        typedef core_tag tag;
    };
}

namespace io {
    template<>
    struct dispatch<rpc::core_tag> {
        typedef boost::mpl::list<
            rpc::heartbeat,
            rpc::terminate,
            rpc::invoke,
            rpc::chunk,
            rpc::error,
            rpc::choke
        >::type category;
    };
}

/*

// Specific packers
// ----------------

template<>
struct command<rpc::invoke>:
    public event_traits<rpc::invoke>::tuple_type
{
    command(const std::string& event, const void * data, size_t size):
        event_traits<rpc::invoke>::tuple_type(event, message),
        message(size)
    {
        memcpy(
            message.data(),
            data,
            size
        );
    }

private:
    zmq::message_t message;
};

template<>
struct command<rpc::chunk>:
    public event_traits<rpc::chunk>::tuple_type
{
    command(const void * data, size_t size):
        event_traits<rpc::chunk>::tuple_type(message),
        message(size)
    {
        memcpy(
            message.data(),
            data,
            size
        );
    }

private:
    zmq::message_t message;
};

template<>
struct command<rpc::error>:
    public event_traits<rpc::error>::tuple_type
{
    command(int code, const std::string& message):
        event_traits<rpc::error>::tuple_type(code, message)
    { }
};

*/
    
} // namespace cocaine

#endif
