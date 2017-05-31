/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2016 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/rpc/asio/encoder.hpp"

#include "cocaine/errors.hpp"

#include "cocaine/hpack/header.hpp"
#include "cocaine/hpack/msgpack_traits.hpp"

#include "cocaine/memory.hpp"

#include "cocaine/rpc/protocol.hpp"

#include "cocaine/trace/trace.hpp"

#include "cocaine/traits.hpp"
#include "cocaine/traits/tuple.hpp"

#include <cstring>

namespace cocaine {
namespace io {

namespace aux {

encoded_buffers_t::encoded_buffers_t() :
offset(0) {
    vector.resize(kInitialBufferSize);
}

void
encoded_buffers_t::write(const char* data, size_t size) {
    size_t new_size = vector.size();
    while (size > new_size - offset) {
        new_size *= 2;
    }
    vector.resize(new_size);

    std::memcpy(vector.data() + offset, data, size);

    offset += size;
}

auto
encoded_buffers_t::data() const -> const char* {
    return vector.data();
}

size_t
encoded_buffers_t::size() const {
    return offset;
}

void
encoded_message_t::write(const char* data, size_t size) {
    return buffer.write(data, size);
}

auto
encoded_message_t::data() const -> const char* {
    return buffer.data();
}

size_t
encoded_message_t::size() const {
    return buffer.size();
}

unbound_message_t::unbound_message_t(function_type&& bind_): bind(std::move(bind_)) { }

} //  namespace aux

void
encoder_t::pack_headers(packer_type& packer, const hpack::header_storage_t& headers) {

    size_t skip = 0;
    for (const auto& header: headers) {
        // Skip packing outdated tracing headers. We use fresh ones (shifted on the tracing tree) from TLS.
        typedef hpack::headers h;
        const auto& name = header.name();
        if (name == h::trace_id<>::name() || name == h::span_id<>::name() || name == h::parent_id<>::name()) {
            skip++;
        }
    }
    packer.pack_array(headers.size() + 3 - skip);

    uint64_t trace_id  = trace_t::current().get_trace_id();
    uint64_t span_id   = trace_t::current().get_id();
    uint64_t parent_id = trace_t::current().get_parent_id();

    hpack::msgpack_traits::pack<hpack::headers::trace_id<>>(packer, hpack_context, hpack::header::pack(trace_id));
    hpack::msgpack_traits::pack<hpack::headers::span_id<>>(packer, hpack_context, hpack::header::pack(span_id));
    hpack::msgpack_traits::pack<hpack::headers::parent_id<>>(packer, hpack_context, hpack::header::pack(parent_id));

    for (const auto& header: headers) {
        // Skip packing outdated tracing headers. We use fresh ones (shifted on the tracing tree) from TLS.
        typedef hpack::headers h;
        const auto& name = header.name();
        if(name == h::trace_id<>::name() || name == h::span_id<>::name() || name == h::parent_id<>::name()) {
            continue;
        }
        hpack::msgpack_traits::pack(packer, hpack_context, header);
    }
}

aux::encoded_message_t
encoder_t::encode(const message_type& message) {
    return message.bind(*this);
}

}} // namespace cocaine::io
