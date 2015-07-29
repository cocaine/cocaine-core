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

#include <asio/generic/stream_protocol.hpp>

#include "cocaine/rpc/asio/encoder.hpp"
#include "cocaine/rpc/asio/decoder.hpp"

namespace cocaine {

class session_t:
    public std::enable_shared_from_this<session_t>
{
    typedef asio::generic::stream_protocol protocol_type;
    typedef protocol_type::endpoint endpoint_type;

    typedef io::transport<protocol_type> transport_type;

    class pull_action_t;
    class push_action_t;

    class channel_t;

    typedef std::map<uint64_t, std::shared_ptr<channel_t>> channel_map_t;

    // Log of last resort.
    const std::unique_ptr<logging::log_t> log;

    // The underlying connection.
#if defined(__clang__)
    std::shared_ptr<transport_type> transport;
#else
    synchronized<std::shared_ptr<transport_type>> transport;
#endif

    // Initial dispatch. Internally synchronized.
    const io::dispatch_ptr_t prototype;

    // Virtual channels.
    synchronized<channel_map_t> channels;

    // The maximum channel id processed by the session. Checking whether channel id is always higher
    // than the previous channel id is similar to an infinite TIME_WAIT timeout for TCP sockets. It
    // might be not the best approach, but since we have 2^64 possible channel ids, and not 2^16 TCP
    // ports available to us, it's good enough.
    uint64_t max_channel_id;

public:
    session_t(std::unique_ptr<logging::log_t> log,
              std::unique_ptr<transport_type> transport, const io::dispatch_ptr_t& prototype);

    // Observers

    auto
    active_channels() const -> std::map<uint64_t, std::string>;

    std::size_t
    memory_pressure() const;

    auto
    name() const -> std::string;

    auto
    remote_endpoint() const -> endpoint_type;

    // Modifiers

    auto
    fork(const io::dispatch_ptr_t& dispatch) -> io::upstream_ptr_t;

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

private:
    void
    handle(const io::decoder_t::message_type& message);

    // NOTE: The revocation happens to channel id only, not the upstream itself. It means that while
    // some channel might be revoked during message handling, it only prohibit new incoming messages
    // from being processed, but shared upstreams still can be used by services to send new outgoing
    // messages to remote peers.

    void
    revoke(uint64_t channel_id);
};

// Defined only for TCP and Local protocols.
template<class Protocol>
class session:
    public session_t
{
public:
    typedef Protocol protocol_type;
    typedef typename protocol_type::endpoint endpoint_type;

    typedef io::transport<protocol_type> transport_type;

public:
    session(std::unique_ptr<logging::log_t> log,
            std::unique_ptr<transport_type> transport, const io::dispatch_ptr_t& prototype);

    auto
    remote_endpoint() const -> endpoint_type;
};

} // namespace cocaine

#endif
