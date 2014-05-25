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

#ifndef COCAINE_IO_DISPATCH_GRAPH_SERIALIZATION_TRAITS_HPP
#define COCAINE_IO_DISPATCH_GRAPH_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"
#include "cocaine/traits/optional.hpp"

#include "cocaine/rpc/graph.hpp"

namespace cocaine { namespace io {

template<>
struct type_traits<dispatch_graph_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const dispatch_graph_t& source) {
        target << static_cast<const dispatch_graph_t::base_type&>(source);
    }

    static inline
    void
    unpack(const msgpack::object& source, dispatch_graph_t& target) {
        source >> static_cast<dispatch_graph_t::base_type&>(target);
    }
};

}} // namespace cocaine::io

#endif
