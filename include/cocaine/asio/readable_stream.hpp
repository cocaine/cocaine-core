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

#ifndef COCAINE_ASIO_READABLE_STREAM_HPP
#define COCAINE_ASIO_READABLE_STREAM_HPP

#include "cocaine/asio/service.hpp"

#include <functional>

namespace cocaine { namespace io {

template<class PipeType>
struct readable_stream:
    boost::noncopyable
{
    readable_stream(service_t& service,
                    const boost::shared_ptr<PipeType>& pipe):
        m_pipe(pipe),
        m_pipe_watcher(service.loop()),
        m_rd_offset(0),
        m_rx_offset(0)
    {
        m_pipe_watcher.set<readable_stream, &readable_stream::on_event>(this);
        m_ring.resize(65536);
    }

    ~readable_stream() {
        BOOST_ASSERT(m_rd_offset == m_rx_offset);
    }

    template<class CallbackType>
    void
    bind(CallbackType callback) {
        m_callback = callback;

        /*
        if(m_rd_offset != m_rx_offset) {
            size_t received = m_callback(
                m_ring.data() + m_rx_offset,
                m_rd_offset - m_rx_offset
            );

            m_rd_offset += received;
        }
        */

        if(!m_pipe_watcher.is_active()) {
            m_pipe_watcher.start(m_pipe->fd(), ev::READ);
        }
    }

    void
    unbind() {
        m_callback = NULL;

        if(m_pipe_watcher.is_active()) {
            m_pipe_watcher.stop();
        }
    }

private:
    void
    on_event(ev::io& io, int revents) {
        while(m_ring.size() - m_rd_offset < 1024) {
            size_t unparsed = m_rd_offset - m_rx_offset;

            if(unparsed + 1024 > m_ring.size()) {
                m_ring.resize(m_ring.size() << 1);
                continue;
            }

            // There's no space left at the end of the buffer, so copy all the unparsed
            // data to the beginning and continue filling it from there.
            std::memcpy(
                m_ring.data(),
                m_ring.data() + m_rx_offset,
                unparsed
            );

            m_rd_offset = unparsed;
            m_rx_offset = 0;
        }

        // Try to read some data.
        ssize_t length = m_pipe->read(
            m_ring.data() + m_rd_offset,
            m_ring.size() - m_rd_offset
        );

        if(length <= 0) {
            if(length == 0) {
                // NOTE: This means that the remote peer has closed the connection.
                m_pipe_watcher.stop();
            }

            return;
        }

        m_rd_offset += length;

        // Try to decode some data.
        size_t parsed = m_callback(
            m_ring.data() + m_rx_offset,
            m_rd_offset - m_rx_offset
        );

        m_rx_offset += parsed;
    }

private:
    // NOTE: Pipes can be shared among multiple queues, at least to be able
    // to write and read from two different queues.
    const boost::shared_ptr<PipeType> m_pipe;

    // Pipe poll object.
    ev::io m_pipe_watcher;

    std::vector<char> m_ring;

    off_t m_rd_offset,
          m_rx_offset;

    std::function<
        size_t(const char*, size_t)
    > m_callback;
};

}} // namespace cocaine::io

#endif
