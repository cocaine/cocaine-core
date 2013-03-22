/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/common.hpp"
#include "cocaine/rpc/message.hpp"

#include <mutex>

namespace cocaine { namespace io {

template<class Stream>
struct encoder:
    boost::noncopyable
{
    typedef Stream stream_type;

    encoder():
        m_packer(m_buffer)
    { }

    ~encoder() {
        // unbind();
    }

    void
    attach(const std::shared_ptr<stream_type>& stream) {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_stream = stream;

        for(message_cache_t::const_iterator it = m_cache.begin();
            it != m_cache.end();
            ++it)
        {
            m_stream->write(it->data(), it->size());
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

        // NOTE: Format is [ID, Band, [Args...]].
        m_packer.pack_array(3);

        m_packer.pack_uint16(traits::id);
        m_packer.pack_uint64(stream);

        type_traits<typename traits::tuple_type>::pack(
            m_packer,
            std::forward<Args>(args)...
        );

        std::unique_lock<std::mutex> lock(m_mutex);

        if(m_stream) {
            m_stream->write(m_buffer.data(), m_buffer.size());
        } else {
            m_cache.emplace_back(m_buffer.data(), m_buffer.size());
        }

        m_buffer.clear();
    }

public:
    std::shared_ptr<stream_type>
    stream() {
        return m_stream;
    }

private:
    msgpack::sbuffer m_buffer;
    msgpack::packer<msgpack::sbuffer> m_packer;

    typedef std::vector<
        std::string
    > message_cache_t;

    // Message cache.
    message_cache_t m_cache;
    std::mutex m_mutex;

    // Attachable stream.
    std::shared_ptr<stream_type> m_stream;
};

}}

#endif
