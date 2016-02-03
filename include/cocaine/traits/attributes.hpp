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

#ifndef COCAINE_LOG_ATTRIBUTE_SERIALIZATION_TRAITS_HPP
#define COCAINE_LOG_ATTRIBUTE_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"

// NOTE: You should manually include the <blackhole/attribute.hpp> header file BEFORE this include.
// Such restrictment is required to avoid exporting Blackhole API.

namespace cocaine { namespace io {

template<>
struct type_traits<blackhole::attribute::value_t> {

    template<class Stream>
    class visitor : public blackhole::attribute::value_t::visitor_t {
        typedef blackhole::attribute::value_t value_t;

        Stream& stream;

    public:
        visitor(Stream& stream) : stream(stream) {}

        virtual auto operator()(const value_t::null_type&) -> void {
            stream.pack_nil();
        }

        virtual auto operator()(const value_t::bool_type& value) -> void {
            stream << value;
        }

        virtual auto operator()(const value_t::sint64_type& value) -> void {
            stream << value;
        }

        virtual auto operator()(const value_t::uint64_type& value) -> void {
            stream << value;
        }

        virtual auto operator()(const value_t::double_type& value) -> void {
            stream << value;
        }

        virtual auto operator()(const value_t::string_type& value) -> void {
            stream << value;
        }

        virtual auto operator()(const value_t::function_type& value) -> void {
            blackhole::writer_t wr;
            value(wr);
            stream.pack_raw(wr.inner.size());
            stream.pack_raw_body(wr.inner.data(), wr.inner.size());
        }
    };

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const blackhole::attribute::value_t& source) {
        visitor<msgpack::packer<Stream>> visitor(target);
        source.apply(visitor);
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

}} // namespace cocaine::io

#endif
