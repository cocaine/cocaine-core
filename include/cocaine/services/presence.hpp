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

#ifndef COCAINE_PRESENCE_SERVICE_INTERFACE_HPP
#define COCAINE_PRESENCE_SERVICE_INTERFACE_HPP

#include "cocaine/rpc/tags.hpp"

namespace cocaine { namespace io {

struct presence_tag;

namespace presence {

struct heartbeat {
    typedef presence_tag  tag;
    typedef recursive_tag transition_type;

    static
    const char*
    alias() {
        return "heartbeat";
    }

    typedef
     /* The UUID of the node, to detect the event of restart. */
        std::string
    result_type;
};

} // namespace presence

template<>
struct protocol<presence_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        presence::heartbeat
    > type;
};

}} // namespace cocaine::io

#endif
