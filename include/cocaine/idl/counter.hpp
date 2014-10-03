/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_SERVICE_COUNTER_INTERFACE_HPP
#define COCAINE_SERVICE_COUNTER_INTERFACE_HPP

#include "cocaine/rpc/protocol.hpp"

namespace cocaine { namespace io {

struct counter_tag;

// Atomic counter service interface

struct counter {

struct inc {
    typedef counter_tag tag;

    static const char* alias() {
        return "inc";
    }

    typedef boost::mpl::list<
        int
    > argument_type;

    typedef stream_of<raft::command_result<int>>::tag upstream_type;
};

struct dec {
    typedef counter_tag tag;

    static const char* alias() {
        return "dec";
    }

    typedef boost::mpl::list<
        int
    > argument_type;

    typedef stream_of<raft::command_result<int>>::tag upstream_type;
};

struct cas {
    typedef counter_tag tag;

    static const char* alias() {
        return "cas";
    }

    typedef boost::mpl::list<
        int,
        int
    > argument_type;

    typedef stream_of<raft::command_result<bool>>::tag upstream_type;
};

}; // struct counter

template<>
struct protocol<counter_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        counter::inc,
        counter::dec,
        counter::cas
    > messages;

    typedef counter scope;
};

}} // namespace cocaine::io

#endif
