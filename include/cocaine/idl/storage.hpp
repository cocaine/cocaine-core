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

#ifndef COCAINE_STORAGE_SERVICE_INTERFACE_HPP
#define COCAINE_STORAGE_SERVICE_INTERFACE_HPP

#include "cocaine/rpc/protocol.hpp"

namespace cocaine { namespace io {

// Storage service interface

struct storage_tag;

struct storage {

struct read {
    typedef storage_tag tag;

    static
    const char*
    alias() {
        return "read";
    }

    typedef boost::mpl::list<
     /* Key namespace. Currently no ACL checks are performed, so in theory any app can read
        any other app data without restrictions. */
        std::string,
     /* Key. */
        std::string
    > tuple_type;

    typedef stream_of<
     /* The stored value. Typically it will be serialized with msgpack, but it's not a strict
        requirement. But as there's no way to know the format, try to unpack it anyway. */
        std::string
    >::tag drain_type;
};

struct write {
    typedef storage_tag tag;

    static
    const char*
    alias() {
        return "write";
    }

    typedef boost::mpl::list<
     /* Key namespace. */
        std::string,
     /* Key. */
        std::string,
     /* Value. Typically, it should be serialized with msgpack, so that the future reader could
        assume that it can be deserialized safely. */
        std::string,
     /* Tag list. Imagine these are your indexes. */
        optional<std::vector<std::string>>
    > tuple_type;
};

struct remove {
    typedef storage_tag tag;

    static
    const char*
    alias() {
        return "remove";
    }

    typedef boost::mpl::list<
     /* Key namespace. Again, due to the lack of ACL checks, any app can obliterate the whole
        storage for all the apps in the cluster. Beware. */
        std::string,
     /* Key. */
        std::string
    > tuple_type;
};

struct find {
    typedef storage_tag tag;

    static
    const char*
    alias() {
        return "find";
    }

    typedef boost::mpl::list<
     /* Key namespace. A good start point to find all the keys to remove to render the system
        useless! Well, one day we'll implement ACLs. */
        std::string,
     /* Tag list. This is actually your query. */
        std::vector<std::string>
    > tuple_type;

    typedef stream_of<
     /* A list of all the keys in the given key namespace. */
        std::vector<std::string>
    >::tag drain_type;
};

}; // struct storage

template<>
struct protocol<storage_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        storage::read,
        storage::write,
        storage::remove,
        storage::find
    > messages;

    typedef storage type;
};

}} // namespace cocaine::io

#endif
