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

#ifndef COCAINE_STREAMING_SERVICE_INTERFACE_HPP
#define COCAINE_STREAMING_SERVICE_INTERFACE_HPP

#include "cocaine/rpc/protocol.hpp"

namespace cocaine { namespace io {

// Streaming service interface

template<class T>
struct streaming {

struct chunk {
    typedef streaming_tag<T> tag;
    typedef recursive_tag    transition_type;

    static
    const char*
    alias() {
        return "write";
    }

    template<class U, class = void>
    struct tuple_type_impl {
        typedef boost::mpl::list<U> type;
    };

    template<class U>
    struct tuple_type_impl<U, typename std::enable_if<std::is_same<U, void>::value>::type> {
        typedef boost::mpl::list<> type;
    };

    template<class U>
    struct tuple_type_impl<U, typename std::enable_if<boost::mpl::is_sequence<U>::value>::type> {
        typedef U type;
    };

    template<typename... Args>
    struct tuple_type_impl<std::tuple<Args...>> {
        typedef typename itemize<Args...>::type type;
    };

    // Automatically expand tuple to a typelist, so that it won't look like a tuple with a single
    // embedded tuple element, i.e. (0, 0, ((42, 3.14, "Death to all humans!"),)).
    typedef typename tuple_type_impl<T>::type tuple_type;
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

struct choke {
    typedef streaming_tag<T> tag;

    // Specifies that this message should seal the stream.
    typedef std::true_type is_sealing;

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
        typename streaming<T>::chunk,
        typename streaming<T>::error,
        typename streaming<T>::choke
    >::type messages;

    typedef streaming<T> type;
};

}} // namespace cocaine::io

#endif
