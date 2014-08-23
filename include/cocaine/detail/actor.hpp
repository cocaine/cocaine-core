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

#include <list>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace cocaine {

class actor_t {
    COCAINE_DECLARE_NONCOPYABLE(actor_t)

    context_t& m_context;

    const std::unique_ptr<logging::log_t> m_log;
    const std::shared_ptr<boost::asio::io_service> m_asio;

    // I/O acceptors. Actors have a separate thread to accept new connections. After a connection
    // is accepted, it is assigned to a random thread from the main thread pool.
    std::list<boost::asio::ip::tcp::acceptor> m_acceptors;

    class accept_action_t;

    // Initial dispatch. It's the protocol dispatch that will be initially assigned to all the new
    // sessions. In case of secure actors, this might as well be the protocol dispatch to switch to
    // after the authentication process completes successfully.
    io::dispatch_ptr_t m_prototype;

    // I/O authentication & processing.
    std::unique_ptr<io::chamber_t> m_chamber;

public:
    actor_t(context_t& context, std::shared_ptr<boost::asio::io_service> asio,
            std::unique_ptr<io::basic_dispatch_t> prototype);

    actor_t(context_t& context, std::shared_ptr<boost::asio::io_service> asio,
            std::unique_ptr<api::service_t> service);

   ~actor_t();

    void
    run(std::vector<boost::asio::ip::tcp::endpoint> endpoints);

    void
    terminate();

public:
    auto
    endpoints() const -> std::vector<boost::asio::ip::tcp::endpoint>;

    auto
    prototype() const -> const io::basic_dispatch_t&;

    bool
    is_active() const;
};

} // namespace cocaine

#endif
