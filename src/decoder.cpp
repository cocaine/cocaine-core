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

#include "cocaine/rpc/asio/decoder.hpp"

#include "cocaine/errors.hpp"
#include "cocaine/hpack/header.hpp"
#include "cocaine/hpack/msgpack_traits.hpp"

#include <msgpack.hpp>

#include <boost/optional/optional.hpp>
#include <boost/range/algorithm/find_if.hpp>

namespace cocaine {
namespace io {

namespace aux {

auto
decoded_message_t::span() const -> uint64_t {
    return object.via.array.ptr[0].as<uint64_t>();
}

auto
decoded_message_t::type() const -> uint64_t {
    return object.via.array.ptr[1].as<uint64_t>();
}

auto
decoded_message_t::args() const -> const msgpack::object& {
    return object.via.array.ptr[2];
}

auto
decoded_message_t::headers() const -> const hpack::headers_t& {
    return metadata;
}

void
decoded_message_t::clear() {
    metadata.clear();
}

} // namespace aux

size_t
decoder_t::decode(const char* data, size_t size, message_type& message, std::error_code& ec) {
    size_t offset = 0;

    // NOTE: We have to clear msgpack zone every decoding iteration to prevent memory leaking
    // for objects structure, because they have no way to notify about self-destruction. Hope
    // someday we migrate to v1.* and everything will be fine automatically.
    zone.clear();

    msgpack::unpack_return rv = msgpack::unpack(data, size, &offset, &zone, &message.object);

    if(rv == msgpack::UNPACK_SUCCESS || rv == msgpack::UNPACK_EXTRA_BYTES) {
        if(message.object.type != msgpack::type::ARRAY || message.object.via.array.size < 3) {
            ec = error::frame_format_error;
        } else if(message.object.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER ||
                  message.object.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER ||
                  message.object.via.array.ptr[2].type != msgpack::type::ARRAY)
        {
            ec = error::frame_format_error;
        } else if(message.object.via.array.size > 3) {
            if(message.object.via.array.ptr[3].type != msgpack::type::ARRAY) {
                ec = error::frame_format_error;
            } else if(!hpack::msgpack_traits::unpack_vector(
                      message.object.via.array.ptr[3], hpack_context, message.metadata))
            {
                ec = error::hpack_error;
            }
        }
    } else if(rv == msgpack::UNPACK_CONTINUE) {
        ec = error::insufficient_bytes;
    } else if(rv == msgpack::UNPACK_PARSE_ERROR) {
        ec = error::parse_error;
    }

    return offset;
}

}} // namespace cocaine::io
