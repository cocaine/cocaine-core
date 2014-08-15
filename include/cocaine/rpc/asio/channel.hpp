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

#include "cocaine/rpc/asio/readable_stream.hpp"
#include "cocaine/rpc/asio/decoder.hpp"

#include "cocaine/rpc/asio/writable_stream.hpp"
#include "cocaine/rpc/asio/encoder.hpp"

namespace cocaine { namespace io {

template<class Protocol>
struct channel {
    typedef Protocol protocol_type;
    typedef boost::asio::basic_stream_socket<protocol_type> socket_type;

    explicit
    channel(std::unique_ptr<socket_type> socket_):
        socket(std::move(socket_)),
        reader(new readable_stream<protocol_type, decoder_t>(socket)),
        writer(new writable_stream<protocol_type, encoder_t>(socket))
    {
        socket->non_blocking(true);
    }

   ~channel() {
        try {
            socket->shutdown(socket_type::shutdown_both);
            socket->close();
        } catch(const boost::system::system_error& e) {
            // Might be already disconnected by the remote peer, so ignore all errors.
        }
    }

    auto
    remote_endpoint() const -> typename protocol_type::endpoint {
        try {
            return socket->remote_endpoint();
        } catch(const boost::system::system_error& e) {
            return typename protocol_type::endpoint();
        }
    }

    // The underlying shared socket object.
    const std::shared_ptr<socket_type> socket;

    // Unidirectional channel streams.
    const std::unique_ptr<readable_stream<protocol_type, decoder_t>> reader;
    const std::unique_ptr<writable_stream<protocol_type, encoder_t>> writer;
};

}} // namespace cocaine::io

#endif
