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

#include <mutex>

#include <boost/asio/ip/tcp.hpp>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/signals2/signal.hpp>

namespace cocaine {

class session_t:
    public std::enable_shared_from_this<session_t>
{
    class channel_t;

    typedef std::map<uint64_t, std::shared_ptr<channel_t>> channel_map_t;

    // NOTE: The underlying connection and session mutex. Upstreams use this mutex to synchronize
    // their state when sending messages, however it does not seem to be under contention.
    std::unique_ptr<io::channel<boost::asio::ip::tcp>> ptr;
    std::mutex mutex;

    class pull_action_t;
    class push_action_t;

    // Keep the remote endpoint in case the socket is closed abruptly and we need to report it.
    const boost::asio::ip::tcp::endpoint endpoint;

    // Initial dispatch.
    const std::shared_ptr<const io::basic_dispatch_t> prototype;

    // Incoming channels counter. It stores the maximum channel id processed by the session. The
    // session assumes that ids of incoming channels are strongly increasing and discards messages
    // with old channel ids.
    uint64_t max_channel;

    // Virtual channels. Separate synchronization to decouple invocation and messaging.
    synchronized<channel_map_t> channels;

public:
    struct {
        boost::signals2::signal<void(const boost::system::error_code&)> collect;
    } signals;

public:
    session_t(std::unique_ptr<io::channel<boost::asio::ip::tcp>> ptr,
              const std::shared_ptr<const io::basic_dispatch_t>& prototype);

    // Channel operations

    void
    invoke(const io::decoder_t::message_type& message);

    std::shared_ptr<io::basic_upstream_t>
    inject(const std::shared_ptr<const io::basic_dispatch_t>& dispatch);

    void
    revoke(uint64_t index);

    // I/O

    void
    pull();

    void
    push(io::encoder_t::message_type&& message);

    // NOTE: Detaching a session destroys the connection but not necessarily the session itself, as
    // it might be still in use by shared upstreams even in other threads. In other words, this does
    // not guarantee that the session will be actually deleted, but it's fine, since the connection
    // is closed.

    void
    detach();

    // Information

    auto
    remote_endpoint() const -> boost::asio::ip::tcp::endpoint;

    auto
    name() const -> std::string;

    auto
    active_channels() const -> std::map<uint64_t, std::string>;
};

} // namespace cocaine

#endif
