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

#ifndef COCAINE_PRIMITIVE_SERVICE_INTERFACE_HPP
#define COCAINE_PRIMITIVE_SERVICE_INTERFACE_HPP

#include "cocaine/rpc/protocol.hpp"

#include <boost/mpl/is_sequence.hpp>

#include <system_error>

namespace cocaine { namespace io {

// Streaming service interface

template<class T>
struct primitive {

static_assert(boost::mpl::is_sequence<T>::value,
    "primitive protocol template argument must be a type sequence");

struct value {
    typedef primitive_tag<T> tag;

    static const char* alias() {
        return "value";
    }

    typedef T    argument_type;
    typedef void upstream_type;
};

struct error {
    typedef primitive_tag<T> tag;

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

}; // struct primitive

template<class T>
struct protocol<primitive_tag<T>> {
    typedef T sequence_type;

    typedef boost::mpl::int_<
        1
    >::type version;

    typedef typename boost::mpl::list<
        typename primitive<T>::value,
        typename primitive<T>::error
    >::type messages;

    typedef primitive<T> scope;
    typedef primitive_tag<T> transition_type;
};

}} // namespace cocaine::io

#endif
