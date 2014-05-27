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

#ifndef COCAINE_IO_CHANNEL_HPP
#define COCAINE_IO_CHANNEL_HPP

#include "cocaine/asio/readable_stream.hpp"
#include "cocaine/asio/writable_stream.hpp"

#include "cocaine/rpc/decoder.hpp"
#include "cocaine/rpc/encoder.hpp"

namespace cocaine { namespace io {

template<class Socket>
struct channel {
    typedef Socket socket_type;

    channel():
        rd(new decoder<readable_stream<socket_type>>()),
        wr(new encoder<writable_stream<socket_type>>())
    { }

    channel(reactor_t& reactor, const std::shared_ptr<socket_type>& socket):
        rd(new decoder<readable_stream<socket_type>>()),
        wr(new encoder<writable_stream<socket_type>>())
    {
        attach(reactor, socket);
    }

    channel(channel&& other):
        rd(std::move(other.rd)),
        wr(std::move(other.wr))
    { }

    channel&
    operator=(channel&& other) {
        rd = std::move(other.rd);
        wr = std::move(other.wr);

        return *this;
    }

    void
    attach(reactor_t& reactor, const std::shared_ptr<socket_type>& socket) {
        rd->attach(std::make_shared<readable_stream<socket_type>>(reactor, socket));
        wr->attach(std::make_shared<writable_stream<socket_type>>(reactor, socket));

        // TODO: Weak pointer, maybe?
        m_socket = socket;
    }

public:
    auto
    remote_endpoint() const -> typename socket_type::endpoint_type {
        return m_socket->remote_endpoint();
    }

    std::unique_ptr<decoder<readable_stream<socket_type>>> rd;
    std::unique_ptr<encoder<writable_stream<socket_type>>> wr;

private:
    std::shared_ptr<socket_type> m_socket;
};

}}

#endif
