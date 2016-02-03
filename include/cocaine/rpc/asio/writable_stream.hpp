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

#ifndef COCAINE_IO_BUFFERED_WRITABLE_STREAM_HPP
#define COCAINE_IO_BUFFERED_WRITABLE_STREAM_HPP

#include "cocaine/errors.hpp"
#include "cocaine/trace/trace.hpp"

#include <functional>

#include <asio/io_service.hpp>
#include <asio/basic_stream_socket.hpp>

#include <deque>

namespace cocaine { namespace io {

template<class Protocol, class Encoder>
class writable_stream:
    public std::enable_shared_from_this<writable_stream<Protocol, Encoder>>
{
    COCAINE_DECLARE_NONCOPYABLE(writable_stream)

    typedef typename Protocol::socket socket_type;

    typedef Encoder encoder_type;
    typedef typename encoder_type::message_type message_type;

    const std::shared_ptr<socket_type> m_socket;

    typedef std::function<void(const std::error_code&)> handler_type;

    std::deque<asio::const_buffer> m_messages;
    std::deque<typename Encoder::encoded_message_type> m_encoded_messages;
    std::deque<handler_type> m_handlers;

    enum class states { idle, flushing } m_state;

    encoder_type encoder;

public:
    explicit
    writable_stream(const std::shared_ptr<socket_type>& socket):
        m_socket(socket),
        m_state(states::idle)
    { }

    void
    write(const message_type& message, handler_type handle) {
        size_t bytes_written = 0;

        auto encoded = encoder.encode(message);

        if(m_state == states::idle) {
            std::error_code ec;

            // Try to write some data right away, as we don't have anything pending.
            bytes_written = m_socket->write_some(asio::buffer(encoded.data(), encoded.size()), ec);

            if(!ec && bytes_written == encoded.size()) {
                return m_socket->get_io_service().post(trace_t::bind(handle, ec));
            }
        }

        m_messages.emplace_back(encoded.data() + bytes_written, encoded.size() - bytes_written);
        m_handlers.emplace_back(handle);
        m_encoded_messages.emplace_back(std::move(encoded));

        if(m_state == states::flushing) {
            return;
        } else {
            m_state = states::flushing;
        }

        namespace ph = std::placeholders;

        m_socket->async_write_some(
            m_messages,
            std::bind(&writable_stream::flush, this->shared_from_this(), ph::_1, ph::_2)
        );
    }

    auto
    pressure() const -> size_t {
        return asio::buffer_size(m_messages);
    }

private:
    void
    flush(const std::error_code& ec, size_t bytes_written) {
        if(ec) {
            if(ec == asio::error::operation_aborted) {
                return;
            }

            while(!m_handlers.empty()) {
                m_socket->get_io_service().post(std::bind(m_handlers.front(), ec));

                m_messages.pop_front();
                m_handlers.pop_front();
                m_encoded_messages.pop_front();
            }

            return;
        }

        while(bytes_written) {
            BOOST_ASSERT(!m_messages.empty() && !m_handlers.empty());

            const size_t message_size = asio::buffer_size(m_messages.front());

            if(message_size > bytes_written) {
                m_messages.front() = m_messages.front() + bytes_written;
                break;
            }

            bytes_written -= message_size;

            // Queue this block's handler for invocation.
            m_socket->get_io_service().post(std::bind(m_handlers.front(), ec));

            m_messages.pop_front();
            m_handlers.pop_front();
            m_encoded_messages.pop_front();
        }

        if(m_messages.empty() && m_state == states::flushing) {
            m_state = states::idle;
            return;
        }

        namespace ph = std::placeholders;

        m_socket->async_write_some(
            m_messages,
            std::bind(&writable_stream::flush, this->shared_from_this(), ph::_1, ph::_2)
        );
    }
};

}} // namespace cocaine::io

#endif
