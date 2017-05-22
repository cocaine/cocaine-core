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

#pragma once

#include "cocaine/common.hpp"
#include "cocaine/locked_ptr.hpp"

#include <asio/ip/tcp.hpp>
#include <asio/local/stream_protocol.hpp>

namespace cocaine {

class actor_t {
public:
    virtual
    auto
    is_active() const -> bool = 0;

    virtual
    auto
    run() -> void = 0;

    virtual
    auto
    terminate() -> void = 0;
};

template<typename Protocol>
class actor_base : public actor_t {
public:
    using protocol_type = Protocol;
    using acceptor_type = typename protocol_type::acceptor;
    using endpoint_type = typename protocol_type::endpoint;

private:
    class accept_action_t;

    context_t& m_context;

    const std::unique_ptr<logging::logger_t> m_log;

    // Initial dispatch. It's the protocol dispatch that will be initially assigned to all the new
    // sessions. In case of secure actors, this might as well be the protocol dispatch to switch to
    // after the authentication process completes successfully. Constant.
    io::dispatch_ptr_t m_prototype;

    // I/O acceptor action. There is a separate thread to accept new connections. After a connection
    // is accepted, it is assigned to a least busy thread from the main thread pool. Synchronized to
    // allow concurrent observing and operations.
    synchronized<std::shared_ptr<accept_action_t>> m_acceptor;

public:
    actor_base(context_t& context, std::unique_ptr<io::basic_dispatch_t> prototype);

    ~actor_base();

    // Observers

    virtual
    auto
    endpoints() const -> std::vector<endpoint_type> = 0;

    virtual
    auto
    prototype() const -> io::dispatch_ptr_t;

    auto
    is_active() const -> bool override;

    // Modifiers

    auto
    run() -> void override;

    auto
    terminate() -> void override;

protected:
    actor_base(context_t& context, io::dispatch_ptr_t prototype);

    auto
    local_endpoint() const -> endpoint_type;

    /// Constructs an endpoint that is used to bind this actor.
    ///
    /// Called once per `run()` to be able to expose a service.
    virtual
    auto
    make_endpoint() const -> endpoint_type = 0;

    /// Called after been run.
    ///
    /// Default implementation does nothing.
    virtual
    auto
    on_run() -> void {}

    /// Called after been terminated.
    ///
    /// Default implementation does nothing.
    virtual
    auto
    on_terminate() -> void {}
};

extern template class actor_base<asio::ip::tcp>;
extern template class actor_base<asio::local::stream_protocol>;

class tcp_actor_t : public actor_base<asio::ip::tcp> {
    context_t& context;

public:
    tcp_actor_t(context_t& context, std::unique_ptr<io::basic_dispatch_t> prototype);
    tcp_actor_t(context_t& context, std::unique_ptr<api::service_t> service);

    auto
    endpoints() const -> std::vector<endpoint_type> override;

protected:
    auto
    make_endpoint() const -> endpoint_type override;

    auto
    on_terminate() -> void override;
};

} // namespace cocaine
