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

#include "cocaine/rpc/protocol.hpp"

#include "cocaine/traits.hpp"
#include "cocaine/traits/tuple.hpp"

#include <cstring>

namespace cocaine { namespace io {

template<class Event>
struct encoded;

struct encoder_t;

namespace aux {

struct encoded_buffers_t {
    friend struct encoded_message_t;

    static const size_t kInitialBufferSize = 2048;

    encoded_buffers_t():
        offset(0)
    {
        vector.resize(kInitialBufferSize);
    }

    void
    write(const char* data, size_t size) {
        while(size > vector.size() - offset) {
            vector.resize(vector.size() * 2);
        }

        std::memcpy(vector.data() + offset, data, size);

        offset += size;
    }

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

    template<class>
    friend struct io::encoded;

    auto
    data() const -> const char* {
        return buffer.vector.data();
    }

    size_t
    size() const {
        return buffer.offset;
    }

private:
    encoded_buffers_t buffer;
};

} // namespace aux

template<class Event>
struct encoded:
    public aux::encoded_message_t
{
    template<class... Args>
    encoded(uint64_t span, Args&&... args) {
        msgpack::packer<aux::encoded_buffers_t> packer(buffer);

        packer.pack_array(3);

        packer.pack(span);
        packer.pack(static_cast<uint64_t>(event_traits<Event>::id));

        typedef typename event_traits<Event>::argument_type argument_type;

        type_traits<argument_type>::pack(packer, std::forward<Args>(args)...);
    }
};

struct encoder_t {
    typedef aux::encoded_message_t message_type;
};

}} // namespace cocaine::io

#endif
