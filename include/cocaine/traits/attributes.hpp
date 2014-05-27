/*
    Copyright (c) 2014+ Evgeny Safronov <division494@gmail.com>
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

#ifndef COCAINE_LOG_PROPERTY_SERIALIZATION_TRAITS_HPP
#define COCAINE_LOG_PROPERTY_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"

#include <blackhole/attribute.hpp>

namespace cocaine { namespace io {

template<>
struct type_traits<blackhole::log::attribute_value_t> {
    static inline
    void
    unpack(const msgpack::object& source, blackhole::log::attribute_value_t& target) {
        switch(source.type) {
        case msgpack::type::RAW:
            target = source.as<std::string>();
            break;
        case msgpack::type::DOUBLE:
            target = source.as<double>();
            break;
        case msgpack::type::POSITIVE_INTEGER:
            target = source.as<uint64_t>();
            break;
        case msgpack::type::NEGATIVE_INTEGER:
            target = source.as<int64_t>();
            break;
        case msgpack::type::BOOLEAN:
            target = source.as<bool>();
            break;
        default:
            throw msgpack::type_error();
        }
    }
};

template<>
struct type_traits<blackhole::log::attributes_t> {
    static inline
    void
    unpack(const msgpack::object& source, blackhole::log::attributes_t& target) {
        if(source.type != msgpack::type::MAP) {
            throw msgpack::type_error();
        }

        msgpack::object_kv *ptr = source.via.map.ptr;
        msgpack::object_kv *const end = ptr + source.via.map.size;

        for(; ptr < end; ++ptr) {
            if(ptr->key.type != msgpack::type::RAW) {
                throw msgpack::type_error();
            }

            const std::string& name = ptr->key.as<std::string>();

            blackhole::log::attribute_value_t value;
            type_traits<blackhole::log::attribute_value_t>::unpack(ptr->val, value);

            target[name] = blackhole::log::attribute_t(value);
        }
    }
 };

 } } // namespace cocaine::io

#endif
