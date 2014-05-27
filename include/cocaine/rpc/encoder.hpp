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

#include "cocaine/rpc/message.hpp"

#include <mutex>

namespace cocaine { namespace io {

template<class Stream>
class encoder {
    COCAINE_DECLARE_NONCOPYABLE(encoder)

    typedef Stream stream_type;

    msgpack::sbuffer m_buffer;
    msgpack::packer<msgpack::sbuffer> m_packer;

    // Message buffer interlocking.
    std::mutex m_mutex;

    // Attachable stream.
    std::shared_ptr<stream_type> m_stream;

public:
    encoder():
        m_packer(m_buffer)
    { }

   ~encoder() {
        if(m_stream) unbind();
    }

    void
    attach(const std::shared_ptr<stream_type>& stream) {
        std::lock_guard<std::mutex> guard(m_mutex);

        m_stream = stream;

        if(m_buffer.size() != 0) {
            m_stream->write(m_buffer.data(), m_buffer.size());
            m_buffer.clear();
        }
    }

    template<class ErrorHandler>
    void
    bind(ErrorHandler error_handler) {
        m_stream->bind(error_handler);
    }

    void
    unbind() {
        m_stream->unbind();
    }

    template<class Event, typename... Args>
    void
    write(uint64_t stream, Args&&... args) {
        typedef event_traits<Event> traits;

        std::lock_guard<std::mutex> guard(m_mutex);

        // NOTE: Format is [ChannelID, MessageID, [Args...]].
        m_packer.pack_array(3);
        m_packer.pack_uint64(stream);
        m_packer.pack_uint32(traits::id);

        type_traits<typename traits::tuple_type>::pack(m_packer, std::forward<Args>(args)...);

        if(m_stream) {
            m_stream->write(m_buffer.data(), m_buffer.size());
            m_buffer.clear();
        }
    }

public:
    auto
    stream() -> std::shared_ptr<stream_type> {
        return m_stream;
    }
};

}}

#endif
