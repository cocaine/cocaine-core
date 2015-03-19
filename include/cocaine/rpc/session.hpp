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

#include <asio/ip/tcp.hpp>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/signals2/signal.hpp>

namespace cocaine {

namespace signals = boost::signals2;

class session_t:
    public std::enable_shared_from_this<session_t>
{
    class channel_t;

    // Discards all the channel dispatches on errors.
    class discard_action_t;

    class pull_action_t;
    class push_action_t;

    typedef std::map<uint64_t, std::shared_ptr<channel_t>> channel_map_t;

    // The underlying connection.
    synchronized<std::shared_ptr<io::channel<asio::ip::tcp>>> transport;

    // Initial dispatch.
    const io::dispatch_ptr_t prototype;

    // The maximum channel id processed by the session. The session assumes that ids of incoming
    // channels are strongly increasing and discards messages with old channel ids.
    uint64_t max_channel_id;

    // Virtual channels.
    synchronized<channel_map_t> channels;

public:
    struct {
        signals::signal<void(const std::error_code&)> shutdown;
    } signals;

public:
    session_t(std::unique_ptr<io::channel<asio::ip::tcp>> transport, const io::dispatch_ptr_t& prototype);

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
    detach();

    // Information

    auto
    active_channels() const -> std::map<uint64_t, std::string>;

    size_t
    memory_pressure() const;

    auto
    name() const -> std::string;

    auto
    remote_endpoint() const -> asio::ip::tcp::endpoint;

private:
    void
    invoke(const io::decoder_t::message_type& message);
};

} // namespace cocaine

#endif
