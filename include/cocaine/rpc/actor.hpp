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

#ifndef COCAINE_ACTOR_HPP
#define COCAINE_ACTOR_HPP

#include "cocaine/common.hpp"
#include "cocaine/locked_ptr.hpp"

#include <asio/ip/tcp.hpp>

namespace cocaine {

class actor_t {
    COCAINE_DECLARE_NONCOPYABLE(actor_t)

    class accept_action_t;

    context_t& m_context;

    const std::unique_ptr<logging::logger_t> m_log;

    struct metrics_t;
    std::unique_ptr<metrics_t> metrics;

    // Initial dispatch. It's the protocol dispatch that will be initially assigned to all the new
    // sessions. In case of secure actors, this might as well be the protocol dispatch to switch to
    // after the authentication process completes successfully. Constant.
    io::dispatch_ptr_t m_prototype;

    // I/O acceptor. Actors have a separate thread to accept new connections. After a connection is
    // is accepted, it is assigned to a least busy thread from the main thread pool. Synchronized to
    // allow concurrent observing and operations.
    synchronized<std::unique_ptr<asio::ip::tcp::acceptor>> m_acceptor;

    // Main service thread.
    std::unique_ptr<io::chamber_t> m_chamber;

public:
    actor_t(context_t& context, std::unique_ptr<io::basic_dispatch_t> prototype);

    actor_t(context_t& context, std::unique_ptr<api::service_t> service);

   ~actor_t();

    // Observers

    auto
    endpoints() const -> std::vector<asio::ip::tcp::endpoint>;

    bool
    is_active() const;

    auto
    prototype() const -> io::dispatch_ptr_t;

    // Modifiers

    void
    run();

    void
    terminate();
};

} // namespace cocaine

#endif
