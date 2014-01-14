/*
    Copyright (c) 2014-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_IO_RAFT_SERIALIZATION_TRAITS_HPP
#define COCAINE_IO_RAFT_SERIALIZATION_TRAITS_HPP

#include "cocaine/detail/raft/forwards.hpp"
#include "cocaine/traits.hpp"
#include "cocaine/traits/frozen.hpp"
#include "cocaine/traits/variant.hpp"

namespace cocaine { namespace io {

template<class StateMachine>
struct type_traits<cocaine::raft::log_entry<StateMachine>> {
    typedef typename cocaine::raft::log_entry<StateMachine>::value_type
            value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const cocaine::raft::log_entry<StateMachine>& source) {
        target.pack_array(2);
        target << source.term();
        type_traits<value_type>::pack(target, source.value());
    }

    static inline
    void
    unpack(const msgpack::object& source, cocaine::raft::log_entry<StateMachine>& target) {
        if(source.type != msgpack::type::ARRAY ||
           source.via.array.size != 2 ||
           source.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER)
        {
            throw std::bad_cast();
        }

        target = cocaine::raft::log_entry<StateMachine>(source.via.array.ptr[0].via.u64, cocaine::raft::nop_t());

        type_traits<value_type>::unpack(source.via.array.ptr[1], target.value());
    }
};

template<>
struct type_traits<cocaine::raft::nop_t> {
    typedef cocaine::raft::nop_t value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const value_type& source) {
        target.pack_nil();
    }

    static inline
    void
    unpack(const msgpack::object& source, value_type& target) {
        if(source.type != msgpack::type::NIL) {
            throw std::bad_cast();
        }
    }
};

}} // namespace cocaine::io

#endif // COCAINE_IO_RAFT_SERIALIZATION_TRAITS_HPP
