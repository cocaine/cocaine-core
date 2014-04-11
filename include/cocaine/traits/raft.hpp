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

#include "cocaine/traits.hpp"
#include "cocaine/traits/frozen.hpp"
#include "cocaine/traits/optional.hpp"
#include "cocaine/traits/tuple.hpp"
#include "cocaine/traits/variant.hpp"

#include "cocaine/detail/raft/forwards.hpp"
#include "cocaine/detail/raft/entry.hpp"

#include <boost/mpl/list.hpp>

namespace cocaine { namespace io {

template<class StateMachine>
struct type_traits<raft::log_entry<StateMachine>> {
    typedef typename raft::log_entry<StateMachine>::command_type value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const raft::log_entry<StateMachine>& source) {
        target.pack_array(2);
        target << source.term();
        type_traits<value_type>::pack(target, source.value());
    }

    static inline
    void
    unpack(const msgpack::object& source, raft::log_entry<StateMachine>& target) {
        if(source.type != msgpack::type::ARRAY ||
           source.via.array.size != 2 ||
           source.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER)
        {
            throw std::bad_cast();
        }

        target = raft::log_entry<StateMachine>(
            source.via.array.ptr[0].via.u64,
            aux::make_frozen<raft::node_commands::nop>()
        );

        type_traits<value_type>::unpack(source.via.array.ptr[1], target.value());
    }
};

template<>
struct type_traits<raft::cluster_config_t> {
    typedef raft::cluster_config_t value_type;
    typedef std::set<raft::node_id_t> topology_t;
    typedef boost::mpl::list<topology_t, boost::optional<topology_t>> sequence_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const value_type& source) {
        type_traits<sequence_type>::pack(target, source.current, source.next);
    }

    static inline
    void
    unpack(const msgpack::object& source, value_type& target) {
        type_traits<sequence_type>::unpack(source, target.current, target.next);
    }
};

template<class T>
struct type_traits<raft::command_result<T>> {
    typedef raft::command_result<T> value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const value_type& source) {
        type_traits<typename value_type::value_type>::pack(target, source.m_value);
    }

    static inline
    void
    unpack(const msgpack::object& source, value_type& target) {
        type_traits<typename value_type::value_type>::unpack(source, target.m_value);
    }
};

template<>
struct type_traits<raft::lockable_config_t> {
    typedef raft::lockable_config_t value_type;
    typedef boost::mpl::list<bool, raft::cluster_config_t> sequence_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const value_type& source) {
        type_traits<sequence_type>::pack(target, source.locked, source.cluster);
    }

    static inline
    void
    unpack(const msgpack::object& source, value_type& target) {
        type_traits<sequence_type>::unpack(source, target.locked, target.cluster);
    }
};

}} // namespace cocaine::io

#endif
