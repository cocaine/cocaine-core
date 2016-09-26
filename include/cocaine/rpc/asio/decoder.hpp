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

#include "cocaine/common.hpp"
#include "cocaine/hpack/header.hpp"

#include <boost/optional/optional.hpp>

#include <msgpack/object.hpp>

namespace cocaine { namespace io {

struct decoder_t;

namespace aux {

struct decoded_message_t {
    friend struct io::decoder_t;

    auto
    span() const -> uint64_t;

    auto
    type() const -> uint64_t;

    auto
    args() const -> const msgpack::object&;

    auto
    headers() const -> const hpack::header_storage_t&;

    void
    clear();

private:

    // These objects keep references to message buffer in the Decoder.
    msgpack::object object;
    hpack::header_storage_t metadata;
};

} // namespace aux

struct decoder_t {
    COCAINE_DECLARE_NONCOPYABLE(decoder_t)

    decoder_t() = default;
   ~decoder_t() = default;

    typedef aux::decoded_message_t message_type;

    size_t
    decode(const char* data, size_t size, message_type& message, std::error_code& ec);

private:
    msgpack::zone zone;

    // HPACK HTTP/2.0 tables.
    hpack::header_table_t hpack_context;
};

}} // namespace cocaine::io

#endif
