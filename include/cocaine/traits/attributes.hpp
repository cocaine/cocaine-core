/*
    Copyright (c) 2014+ Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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
#include <blackhole/formatter/msgpack.hpp>

namespace cocaine { namespace io {

template<>
struct type_traits<blackhole::attribute::value_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const blackhole::attribute::value_t& source) {
        blackhole::formatter::msgpack_visitor<Stream> visitor(&packer);
        boost::apply_visitor(visitor, source);
    }

    static inline
    void
    unpack(const msgpack::object& source, blackhole::attribute::value_t& target) {
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
struct type_traits<blackhole::attribute_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const blackhole::attribute_t& source) {
        type_traits<blackhole::attribute::value_t>::pack(packer, source.value);
    }

    static inline
    void
    unpack(const msgpack::object& source, blackhole::attribute_t& target) {
        type_traits<blackhole::attribute::value_t>::unpack(source, target.value);
    }
};

}} // namespace cocaine::io

#endif
