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

#ifndef COCAINE_IO_SESSION_HPP
#define COCAINE_IO_SESSION_HPP

#include "cocaine/common.hpp"
#include "cocaine/locked_ptr.hpp"

#include <mutex>

namespace cocaine {

class session_t:
    public std::enable_shared_from_this<session_t>
{
    class channel_t;

    // NOTE: The underlying connection and session mutex. Upstreams use this mutex to synchronize
    // their state when sending messages, however it does not seem to be under contention.
    std::unique_ptr<io::channel<io::socket<io::tcp>>> ptr;
    std::mutex mutex;

    // Initial dispatch.
    const std::shared_ptr<io::dispatch_t> prototype;

    // Virtual channels.
    typedef std::map<uint64_t, std::shared_ptr<channel_t>> channel_map_t;

    // NOTE: Virtual channels use their own synchronization to decouple invocation and messaging.
    synchronized<channel_map_t> channels;

public:
    friend class upstream_t;

    session_t(std::unique_ptr<io::channel<io::socket<io::tcp>>>&& ptr_, const std::shared_ptr<io::dispatch_t>& prototype_):
        ptr(std::move(ptr_)),
        prototype(prototype_)
    { }

    void
    invoke(const io::message_t& message);

    void
    detach();

private:
    void
    revoke(uint64_t index);
};

} // namespace cocaine

#endif
