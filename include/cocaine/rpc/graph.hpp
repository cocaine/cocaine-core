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

#ifndef COCAINE_IO_DISPATCH_GRAPH_HPP
#define COCAINE_IO_DISPATCH_GRAPH_HPP

#include <map>
#include <string>
#include <tuple>

#include <boost/optional/optional_fwd.hpp>

namespace cocaine { namespace io {

struct graph_node_t;

namespace aux {

// Protocol transitions are described by an optional<graph_node_t>. Transition could be a new graph
// point, an empty graph point (terminal message) or none, which means recurrent transition.

typedef std::map<
    int,
    std::tuple<std::string, boost::optional<graph_node_t>>
> recursion_base_t;

} // namespace aux

struct graph_node_t: public aux::recursion_base_t {
    typedef aux::recursion_base_t base_type;
};

typedef std::map<
    int,
    std::tuple<std::string, boost::optional<graph_node_t>, boost::optional<graph_node_t>>
> graph_root_t;

}} // namespace cocaine::io

#endif
