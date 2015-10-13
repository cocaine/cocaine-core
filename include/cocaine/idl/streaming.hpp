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

#ifndef COCAINE_STREAMING_SERVICE_INTERFACE_HPP
#define COCAINE_STREAMING_SERVICE_INTERFACE_HPP

#include "cocaine/rpc/protocol.hpp"

#include <boost/mpl/is_sequence.hpp>

namespace cocaine { namespace io {

// Streaming service interface

template<class T>
struct streaming {

static_assert(
    boost::mpl::is_sequence<T>::value,
    "streaming protocol template argument must be a type sequence"
);

struct chunk {
    typedef streaming_tag<T> tag;
    typedef streaming_tag<T> dispatch_type;

    static const char* alias() {
        return "write";
    }

    typedef T    argument_type;
    typedef void upstream_type;
};

struct error {
    typedef streaming_tag<T> tag;

    static const char* alias() {
        return "error";
    }

    typedef boost::mpl::list<
     /* Serialized error category and error code. */
        std::error_code,
     /* Specially crafted personal error message. */
        optional<std::string>
    >::type argument_type;

    // Terminal message.
    typedef void upstream_type;
};

struct choke {
    typedef streaming_tag<T> tag;

    static const char* alias() {
        return "close";
    }

    // Terminal message.
    typedef void upstream_type;
};

}; // struct streaming

template<class T>
struct protocol<streaming_tag<T>> {
    typedef T sequence_type;

    typedef boost::mpl::int_<
        1
    >::type version;

    typedef typename boost::mpl::list<
        typename streaming<T>::chunk,
        typename streaming<T>::error,
        typename streaming<T>::choke
    >::type messages;

    typedef streaming<T> scope;
};

}} // namespace cocaine::io

#endif
