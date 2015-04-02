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

#ifndef COCAINE_IO_SESSION_HPP
#define COCAINE_IO_SESSION_HPP

#include "cocaine/common.hpp"
#include "cocaine/locked_ptr.hpp"

#include "cocaine/rpc/asio/encoder.hpp"
#include "cocaine/rpc/asio/decoder.hpp"

#include <asio/generic/stream_protocol.hpp>

namespace cocaine {

class session_t:
    public std::enable_shared_from_this<session_t>
{
    typedef asio::generic::stream_protocol protocol_type;

    class pull_action_t;
    class push_action_t;

    class channel_t;

    typedef std::map<uint64_t, std::shared_ptr<channel_t>> channel_map_t;

    // Log of last resort.
    const std::unique_ptr<logging::log_t> log;

    // The underlying connection.
#if defined(__clang__)
    std::shared_ptr<io::channel<protocol_type>> transport;
#else
    synchronized<std::shared_ptr<io::channel<protocol_type>>> transport;
#endif

    // Initial dispatch. Internally synchronized.
    const io::dispatch_ptr_t prototype;

    // Virtual channels.
    synchronized<channel_map_t> channels;

    // The maximum channel id processed by the session. The session assumes that ids of incoming
    // channels are strongly increasing and discards messages with old channel ids.
    uint64_t max_channel_id;

public:
    session_t(std::unique_ptr<logging::log_t> log,
              std::unique_ptr<io::channel<protocol_type>> transport,
              const io::dispatch_ptr_t& prototype);

    // Operations

    auto
    inject(const io::dispatch_ptr_t& dispatch) -> io::upstream_ptr_t;

    void
    revoke(uint64_t channel_id);

    // Channel I/O

    void
    pull();

    void
    push(io::encoder_t::message_type&& message);

    // NOTE: Detaching a session destroys the connection but not necessarily the session itself, as
    // it might be still in use by shared upstreams even in other threads. In other words, this does
    // not guarantee that the session will be actually deleted, but it's fine, since the connection
    // is closed.

    void
    detach(const std::error_code& ec);

    // Information

    auto
    active_channels() const -> std::map<uint64_t, std::string>;

    size_t
    memory_pressure() const;

    auto
    name() const -> std::string;

    auto
    remote_endpoint() const -> protocol_type::endpoint;

private:
    void
    invoke(const io::decoder_t::message_type& message);
};

} // namespace cocaine

#include <asio/ip/tcp.hpp>
#include <boost/lexical_cast.hpp>
namespace std {

inline
std::string
to_string(const asio::generic::stream_protocol::endpoint& endpoint) {
    // TODO: Это пиздец.
    switch (endpoint.protocol().family()) {
    case AF_INET: {
        const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(endpoint.data());
        asio::ip::address_v4::bytes_type array;
        std::copy((char*)&addr->sin_addr, (char*)&addr->sin_addr + array.size(), array.begin());
        asio::ip::address_v4 address(array);
        return boost::lexical_cast<std::string>(
            asio::ip::tcp::endpoint(
                address,
                asio::detail::socket_ops::network_to_host_short(addr->sin_port)
            )
        );
    }
    case AF_INET6: {
        const sockaddr_in6* addrv6 = reinterpret_cast<const sockaddr_in6*>(endpoint.data());
        asio::ip::address_v6::bytes_type array;
        std::copy((char*)&addrv6->sin6_addr, (char*)&addrv6->sin6_addr + array.size(), array.begin());
        asio::ip::address_v6 address(array, addrv6->sin6_scope_id);
        return boost::lexical_cast<std::string>(
            asio::ip::tcp::endpoint(
                address,
                asio::detail::socket_ops::network_to_host_short(addrv6->sin6_port)
            )
        );
    }
    case AF_UNIX: {
        const sockaddr_un* addr = (const sockaddr_un*)(endpoint.data());
        return std::string(addr->sun_path, addr->sun_len);
    }
    default:
        break;
    };

    return "<unknown protocol type>";
}

} // namespace std

#endif
