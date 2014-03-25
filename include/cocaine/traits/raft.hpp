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
#include "cocaine/detail/raft/entry.hpp"
#include "cocaine/detail/raft/configuration.hpp"
#include "cocaine/traits.hpp"
#include "cocaine/traits/frozen.hpp"
#include "cocaine/traits/variant.hpp"
#include "cocaine/traits/tuple.hpp"
#include "cocaine/traits/optional.hpp"

#include <boost/mpl/list.hpp>

namespace cocaine { namespace io {

template<class StateMachine>
struct type_traits<cocaine::raft::log_entry<StateMachine>> {
    typedef typename cocaine::raft::log_entry<StateMachine>::command_type
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

template<>
struct type_traits<cocaine::raft::insert_node_t> {
    typedef cocaine::raft::insert_node_t value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const value_type& source) {
        type_traits<cocaine::raft::node_id_t>::pack(target, source.node);
    }

    static inline
    void
    unpack(const msgpack::object& source, value_type& target) {
        type_traits<cocaine::raft::node_id_t>::unpack(source, target.node);
    }
};

template<>
struct type_traits<cocaine::raft::erase_node_t> {
    typedef cocaine::raft::erase_node_t value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const value_type& source) {
        type_traits<cocaine::raft::node_id_t>::pack(target, source.node);
    }

    static inline
    void
    unpack(const msgpack::object& source, value_type& target) {
        type_traits<cocaine::raft::node_id_t>::unpack(source, target.node);
    }
};

template<>
struct type_traits<cocaine::raft::commit_node_t> {
    typedef cocaine::raft::commit_node_t value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const value_type& source) {
        type_traits<std::tuple<>>::pack(target, std::tuple<>());
    }

    static inline
    void
    unpack(const msgpack::object& source, value_type& target) {
        std::tuple<> targ;
        type_traits<std::tuple<>>::unpack(source, targ);
    }
};

template<>
struct type_traits<cocaine::raft::cluster_config_t> {
    typedef cocaine::raft::cluster_config_t value_type;
    typedef boost::mpl::list<std::set<cocaine::raft::node_id_t>,
                             boost::optional<std::set<cocaine::raft::node_id_t>>>
            seq_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const value_type& source) {
        type_traits<seq_type>::pack(target, source.current, source.next);
    }

    static inline
    void
    unpack(const msgpack::object& source, value_type& target) {
        type_traits<seq_type>::unpack(source, target.current, target.next);
    }
};

template<class T>
struct type_traits<cocaine::raft::command_result<T>> {
    typedef cocaine::raft::command_result<T> value_type;

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

}} // namespace cocaine::io

#endif // COCAINE_IO_RAFT_SERIALIZATION_TRAITS_HPP
