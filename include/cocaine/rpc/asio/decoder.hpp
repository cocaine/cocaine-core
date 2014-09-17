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

#ifndef COCAINE_IO_DECODER_HPP
#define COCAINE_IO_DECODER_HPP

#include "cocaine/rpc/asio/errors.hpp"

#include "cocaine/traits.hpp"

namespace cocaine { namespace io {

struct decoder_t;

namespace aux {

struct decoded_message_t {
    friend struct io::decoder_t;

    uint64_t
    span() const {
        return object.via.array.ptr[0].as<uint64_t>();
    }

    uint32_t
    type() const {
        return object.via.array.ptr[1].as<uint32_t>();
    }

    const msgpack::object&
    args() const {
        return object.via.array.ptr[2];
    }

private:
    msgpack::object object;
};

} // namespace aux

struct decoder_t {
    COCAINE_DECLARE_NONCOPYABLE(decoder_t)

    decoder_t() = default;
   ~decoder_t() = default;

    typedef aux::decoded_message_t message_type;

    size_t
    decode(const char* data, size_t size, message_type& message, boost::system::error_code& ec) {
        size_t offset = 0;

        msgpack::unpack_return rv = msgpack::unpack(data, size, &offset, &zone, &message.object);

        if(rv == msgpack::UNPACK_CONTINUE) {
            ec = error::insufficient_bytes;
        } else if(rv == msgpack::UNPACK_PARSE_ERROR) {
            ec = error::parse_error;
        } else if(message.object.type != msgpack::type::ARRAY || message.object.via.array.size != 3) {
            ec = error::frame_format_error;
        } else if(message.object.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER ||
                  message.object.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER ||
                  message.object.via.array.ptr[2].type != msgpack::type::ARRAY)
        {
            ec = error::frame_format_error;
        }

        return offset;
    }

private:
    msgpack::zone zone;
};

}} // namespace cocaine::io

#endif
