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

#ifndef COCAINE_IO_ENCODER_HPP
#define COCAINE_IO_ENCODER_HPP

#include "cocaine/hpack/header.hpp"
#include "cocaine/memory.hpp"
#include "cocaine/rpc/protocol.hpp"
#include "cocaine/traits/tuple.hpp"

namespace cocaine { namespace io {

template<class Event>
struct encoded;

struct encoder_t;

namespace aux {

struct encoded_buffers_t {
    friend struct encoded_message_t;

    static const size_t kInitialBufferSize = 2048;

    encoded_buffers_t();

    void
    write(const char* data, size_t size);

    // Movable

    encoded_buffers_t(encoded_buffers_t&&) = default;

    encoded_buffers_t&
    operator=(encoded_buffers_t&&) = default;

    COCAINE_DECLARE_NONCOPYABLE(encoded_buffers_t)

private:
    std::vector<char, uninitialized<char>> vector;
    std::vector<char, uninitialized<char>>::size_type offset;
};

struct encoded_message_t {
    friend struct io::encoder_t;

    auto
    data() const -> const char*;

    size_t
    size() const;

private:
    encoded_buffers_t buffer;
};

struct unbound_message_t {
    typedef std::function<aux::encoded_message_t(encoder_t&)> function_type;

    // Partially applied message encoding function.
    const function_type bind;

    unbound_message_t(function_type&& bind_);
};

} // namespace aux

struct encoder_t {
    COCAINE_DECLARE_NONCOPYABLE(encoder_t)

    encoder_t() = default;
   ~encoder_t() = default;

    typedef aux::unbound_message_t message_type;
    typedef aux::encoded_message_t encoded_message_type;
    typedef msgpack::packer<aux::encoded_buffers_t> packer_type;

    // TODO: Do we really need owning header storage?
    template<class Event, class... Args>
    static inline
    aux::encoded_message_t
    tether(encoder_t& encoder, uint64_t channel_id, const hpack::header_storage_t& headers, Args&... args) {
        aux::encoded_message_t message;

        packer_type packer(message.buffer);

        packer.pack_array(4);

        // Channel ID & Message ID

        packer.pack(channel_id);
        packer.pack(static_cast<uint64_t>(event_traits<Event>::id));

        // Message arguments

        type_traits<typename event_traits<Event>::argument_type>::pack(packer,
            std::forward<Args>(args)...);

        encoder.pack_headers(packer, headers);
        return message;
    }

    aux::encoded_message_t
    encode(const message_type& message);

    void
    pack_headers(packer_type& packer, const hpack::header_storage_t& headers);

private:
    // HPACK HTTP/2.0 tables.
    hpack::header_table_t hpack_context;
};

template<class Event>
struct encoded:
    public aux::unbound_message_t
{
    template<class... Args>
    encoded(uint64_t channel_id, Args&&... args): unbound_message_t(
    std::bind(&encoder_t::tether<Event, typename std::decay<Args>::type...>,
              std::placeholders::_1,
              channel_id,
              hpack::header_storage_t(),
              std::forward<Args>(args)...))
    { }

    template<class... Args>
    encoded(uint64_t channel_id, hpack::header_storage_t headers, Args&&... args): unbound_message_t(
        std::bind(&encoder_t::tether<Event, typename std::decay<Args>::type...>,
            std::placeholders::_1,
            channel_id,
            std::move(headers),
            std::forward<Args>(args)...))
    { }
};

}} // namespace cocaine::io

#endif
