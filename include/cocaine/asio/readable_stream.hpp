/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_IO_BUFFERED_READABLE_STREAM_HPP
#define COCAINE_IO_BUFFERED_READABLE_STREAM_HPP

#include "cocaine/asio/reactor.hpp"

#include <cstring>

namespace cocaine { namespace io {

template<class Socket>
struct readable_stream {
    COCAINE_DECLARE_NONCOPYABLE(readable_stream)

    typedef Socket socket_type;
    typedef typename socket_type::endpoint_type endpoint_type;

    readable_stream(reactor_t& reactor, endpoint_type endpoint):
        m_socket(std::make_shared<socket_type>(endpoint)),
        m_socket_watcher(reactor.native()),
        m_rd_offset(0),
        m_rx_offset(0)
    {
        m_socket_watcher.set<readable_stream, &readable_stream::on_event>(this);
        m_ring.resize(65536);
    }

    readable_stream(reactor_t& reactor, const std::shared_ptr<socket_type>& socket):
        m_socket(socket),
        m_socket_watcher(reactor.native()),
        m_rd_offset(0),
        m_rx_offset(0)
    {
        m_socket_watcher.set<readable_stream, &readable_stream::on_event>(this);
        m_ring.resize(65536);
    }

   ~readable_stream() {
        BOOST_ASSERT(m_rd_offset == m_rx_offset);
    }

    template<class ReadHandler, class ErrorHandler>
    void
    bind(ReadHandler read_handler,
         ErrorHandler error_handler)
    {
        if(!m_socket_watcher.is_active()) {
            m_socket_watcher.start(m_socket->fd(), ev::READ);
        }

        m_handle_read = read_handler;
        m_handle_error = error_handler;

        /*
        if(m_rd_offset != m_rx_offset) {
            size_t received = m_handle_read(
                m_ring.data() + m_rx_offset,
                m_rd_offset - m_rx_offset
            );

            m_rd_offset += received;
        }
        */
    }

    void
    unbind() {
        if(m_socket_watcher.is_active()) {
            m_socket_watcher.stop();
        }

        m_handle_read = nullptr;
        m_handle_error = nullptr;
    }

private:
    void
    on_event(ev::io& /* io */, int /* revents */) {
        while(m_ring.size() - m_rd_offset < 1024) {
            size_t unparsed = m_rd_offset - m_rx_offset;

            if(unparsed + 1024 > m_ring.size()) {
                m_ring.resize(m_ring.size() << 1);
                continue;
            }

            // There's no space left at the end of the buffer, so copy all the unparsed
            // data to the beginning and continue filling it from there.
            std::memmove(
                m_ring.data(),
                m_ring.data() + m_rx_offset,
                unparsed
            );

            m_rd_offset = unparsed;
            m_rx_offset = 0;
        }

        // Keep the error code if the read() operation fails.
        std::error_code ec;

        // Try to read some data.
        ssize_t length = m_socket->read(
            m_ring.data() + m_rd_offset,
            m_ring.size() - m_rd_offset,
            ec
        );

        if(ec) {
            m_handle_error(ec);
            return;
        }

        if(length <= 0) {
            if(length == 0) {
                // NOTE: This means that the remote peer has closed the connection.
                m_socket_watcher.stop();
            }

            return;
        }

        m_rd_offset += length;

        // Try to decode some data.
        size_t parsed = m_handle_read(
            m_ring.data() + m_rx_offset,
            m_rd_offset - m_rx_offset
        );

        m_rx_offset += parsed;
    }

private:
    // NOTE: Sockets can be shared among multiple queues, at least to be able
    // to write and read from two different queues.
    const std::shared_ptr<socket_type> m_socket;

    // Socket poll object.
    ev::io m_socket_watcher;

    std::vector<char> m_ring;

    off_t m_rd_offset,
          m_rx_offset;

    // Socket data callback.
    std::function<
        size_t(const char*, size_t)
    > m_handle_read;

    // Socket error callback.
    std::function<
        void(const std::error_code&)
    > m_handle_error;
};

}} // namespace cocaine::io

#endif
