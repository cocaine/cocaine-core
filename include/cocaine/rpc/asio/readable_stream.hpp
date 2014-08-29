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

#ifndef COCAINE_IO_BUFFERED_READABLE_STREAM_HPP
#define COCAINE_IO_BUFFERED_READABLE_STREAM_HPP

#include "cocaine/rpc/asio/errors.hpp"

#include <functional>

#include <boost/asio/io_service.hpp>
#include <boost/asio/basic_stream_socket.hpp>

#include <cstring>

namespace cocaine { namespace io {

template<class Protocol, class Decoder>
class readable_stream {
    COCAINE_DECLARE_NONCOPYABLE(readable_stream)

    static const size_t kInitialBufferSize = 65536;

    typedef boost::asio::basic_stream_socket<Protocol> channel_type;

    typedef Decoder decoder_type;
    typedef typename decoder_type::message_type message_type;

    const std::shared_ptr<channel_type> m_channel;

    typedef std::function<void(const boost::system::error_code&)> handler_type;

    std::vector<char> m_ring;
    std::vector<char>::difference_type m_rd_offset;

    decoder_type m_decoder;

public:
    explicit
    readable_stream(const std::shared_ptr<channel_type>& channel):
        m_channel(channel)
    {
        m_ring.resize(kInitialBufferSize);
        m_rd_offset = 0;
    }

    void
    read(message_type& message, handler_type handle) {
        boost::system::error_code ec;

        const size_t
            bytes_parsed  = m_decoder.decode(m_ring.data(), m_rd_offset, message, ec),
            bytes_pending = m_rd_offset - bytes_parsed;

        if(ec != error::insufficient_bytes) {
            if(bytes_parsed) {
                // Compactify the ring.
                std::memmove(m_ring.data(), m_ring.data() + bytes_parsed, bytes_pending);
                m_rd_offset = bytes_pending;
            }

            return m_channel->get_io_service().post(std::bind(handle, ec));
        }

        if(bytes_pending > m_ring.size() / 2) {
            // The total size of unprocessed data in larger than half the size of the ring, so grow
            // the ring in order to accomodate more data.
            m_ring.resize(m_ring.size() * 2);
        }

        using namespace std::placeholders;

        m_channel->async_read_some(
            boost::asio::buffer(m_ring.data() + m_rd_offset, m_ring.size() - m_rd_offset),
            std::bind(&readable_stream::fill, this, std::ref(message), handle, _1, _2)
        );
    }

private:
    void
    fill(message_type& message, handler_type handle, const boost::system::error_code& ec, size_t bytes_read) {
        if(ec) {
            BOOST_ASSERT(!bytes_read);

            if(ec == boost::asio::error::operation_aborted) {
                return;
            }

            return m_channel->get_io_service().post(std::bind(handle, ec));
        }

        m_rd_offset += bytes_read;

        read(std::ref(message), handle);
    }
};

}} // namespace cocaine::io

#endif
