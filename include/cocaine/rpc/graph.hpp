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

#ifndef COCAINE_IO_DISPATCH_GRAPH_HPP
#define COCAINE_IO_DISPATCH_GRAPH_HPP

#include <map>
#include <string>
#include <tuple>

#include <boost/optional.hpp>

namespace cocaine { namespace io {

struct dispatch_graph_t;

namespace aux {

typedef std::map<
    /* message id */ int,
    std::tuple<
        /* message alias */ std::string,
        /* transition graph */ boost::optional<dispatch_graph_t>,
        /* drain graph */ boost::optional<dispatch_graph_t>
    >
> recursion_base_t;

} // namespace aux

struct dispatch_graph_t: public aux::recursion_base_t {
    typedef aux::recursion_base_t base_type;

    dispatch_graph_t() { }
    dispatch_graph_t(const dispatch_graph_t& o): base_type(o) { }
    dispatch_graph_t(dispatch_graph_t&& o): base_type(std::move(o)) { }

    dispatch_graph_t& operator=(const dispatch_graph_t& rhs) {
        static_cast<base_type&>(*this) = rhs; return *this;
    }

    dispatch_graph_t& operator=(dispatch_graph_t&& rhs) {
        static_cast<base_type&>(*this) = std::move(rhs); return *this;
    }
};

}} // namespace cocaine::io

#endif
