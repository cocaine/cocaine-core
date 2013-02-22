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

#ifndef COCAINE_ASIO_WRITABLE_STREAM_HPP
#define COCAINE_ASIO_WRITABLE_STREAM_HPP

#include "cocaine/asio/service.hpp"

#include <cstring>
#include <mutex>

namespace cocaine { namespace io {

template<class Stream>
struct writable_stream:
    boost::noncopyable
{
    typedef Stream stream_type;
    typedef typename stream_type::endpoint_type endpoint_type;

    writable_stream(service_t& service,
                    endpoint_type endpoint):
        m_stream(std::make_shared<stream_type>(endpoint)),
        m_stream_watcher(service.loop()),
        m_tx_offset(0),
        m_wr_offset(0)
    {
        m_stream_watcher.set<writable_stream, &writable_stream::on_event>(this);
        m_ring.resize(65536);
    }

    writable_stream(service_t& service,
                    const std::shared_ptr<stream_type>& stream):
        m_stream(stream),
        m_stream_watcher(service.loop()),
        m_tx_offset(0),
        m_wr_offset(0)
    {
        m_stream_watcher.set<writable_stream, &writable_stream::on_event>(this);
        m_ring.resize(65536);
    }

    ~writable_stream() {
        BOOST_ASSERT(m_tx_offset == m_wr_offset);
    }

    void
    write(const char * data,
          size_t size)
    {
        std::unique_lock<std::mutex> m_lock(m_ring_mutex);

        if(m_tx_offset == m_wr_offset) {
            // Nothing is pending in the ring so try to write directly to the stream,
            // and enqueue only the remaining part, if any.
            ssize_t sent = m_stream->write(data, size);

            if(sent >= 0) {
                if(static_cast<size_t>(sent) == size) {
                    return;
                }

                data += sent;
                size -= sent;
            }
        }

        while(m_ring.size() - m_wr_offset < size) {
            size_t unsent = m_wr_offset - m_tx_offset;

            if(unsent + size > m_ring.size()) {
                m_ring.resize(m_ring.size() << 1);
                continue;
            }

            // There's no space left at the end of the buffer, so copy all the unsent
            // data to the beginning and continue filling it from there.
            std::memcpy(
                m_ring.data(),
                m_ring.data() + m_tx_offset,
                unsent
            );

            m_wr_offset = unsent;
            m_tx_offset = 0;
        }

        std::memcpy(m_ring.data() + m_wr_offset, data, size);

        m_wr_offset += size;

        if(!m_stream_watcher.is_active()) {
            m_stream_watcher.start(m_stream->fd(), ev::WRITE);
        }
    }

private:
    void
    on_event(ev::io& /* io */, int /* revents */) {
        std::unique_lock<std::mutex> m_lock(m_ring_mutex);

        if(m_tx_offset == m_wr_offset) {
            m_stream_watcher.stop();
            return;
        }

        size_t unsent = m_wr_offset - m_tx_offset;

        // Try to send all the data at once.
        ssize_t sent = m_stream->write(
            m_ring.data() + m_tx_offset,
            unsent
        );

        if(sent > 0) {
            m_tx_offset += sent;
        }
    }

private:
    // NOTE: Streams can be shared among multiple queues, at least to be able
    // to write and read from two different queues.
    const std::shared_ptr<stream_type> m_stream;

    // Stream poll object.
    ev::io m_stream_watcher;

    std::vector<char> m_ring;

    off_t m_tx_offset,
          m_wr_offset;

    std::mutex m_ring_mutex;
};

}} // namespace cocaine::io

#endif
