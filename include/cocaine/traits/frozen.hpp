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

#ifndef COCAINE_IO_FROZEN_SERIALIZATION_TRAITS_HPP
#define COCAINE_IO_FROZEN_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"

#include "cocaine/rpc/queue.hpp"

namespace cocaine { namespace io {

template<class Event>
struct type_traits<frozen<Event>> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const frozen<Event>& source) {
        type_traits<typename frozen<Event>::tuple_type>::pack(target, source.tuple);
    }

    static inline
    void
    unpack(const msgpack::object& source, frozen<Event>& target) {
        type_traits<typename frozen<Event>::tuple_type>::unpack(source, target.tuple);
    }
};

}} // namespace cocaine::io

#endif
