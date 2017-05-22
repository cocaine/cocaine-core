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

#include "cocaine/rpc/actor.hpp"

#include "cocaine/api/service.hpp"

#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/context/mapper.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/engine.hpp"

#include "cocaine/rpc/basic_dispatch.hpp"

#include <asio/local/stream_protocol.hpp>

#include <blackhole/logger.hpp>

#include <metrics/registry.hpp>

#include "chamber.hpp"

using namespace cocaine;
using namespace cocaine::io;

using namespace asio;
using ip::tcp;
using local::stream_protocol;

namespace ph = std::placeholders;

// Actor internals

struct metrics_t {
    metrics::shared_metric<std::atomic<std::int64_t>> connections_accepted;
    metrics::shared_metric<std::atomic<std::int64_t>> connections_rejected;

    metrics_t(context_t& context, const std::string& name) :
        connections_accepted(context.metrics_hub().counter<std::int64_t>(format("{}.connections.accepted", name))),
        connections_rejected(context.metrics_hub().counter<std::int64_t>(format("{}.connections.rejected", name)))
    {}
};

template<typename Protocol>
class actor_base<Protocol>::accept_action_t:
    public std::enable_shared_from_this<accept_action_t>
{
public:
    using protocol_type = Protocol;
    using acceptor_type = typename protocol_type::acceptor;
    using endpoint_type = typename protocol_type::endpoint;
    using parent_type = actor_base<protocol_type>;
    using socket_type = typename protocol_type::socket;

private:
    context_t& context;
    // Reference to an event loop tasks are running on.
    asio::io_service& loop;
    socket_type socket;
    std::unique_ptr<acceptor_type> acceptor;
    endpoint_type m_local_endpoint;
    io::dispatch_ptr_t prototype;
    metrics_t metrics;
    std::unique_ptr<logging::logger_t> log;

public:
    accept_action_t(parent_type& parent, std::unique_ptr<acceptor_type> acceptor):
        context(parent.m_context),
        loop(acceptor->get_io_service()),
        socket(loop),
        acceptor(std::move(acceptor)),
        m_local_endpoint(this->acceptor->local_endpoint()),
        prototype(parent.m_prototype),
        metrics(context, prototype->name()),
        log(context.log("core/asio", {{"service", parent.m_prototype->name()}}))
    {}

    void
    cancel() {
        loop.post([&] {
            acceptor->cancel();
        });
    }

    auto
    local_endpoint() const -> endpoint_type {
        return m_local_endpoint;
    }

    void
    run() {
        acceptor->async_accept(
            socket,
            std::bind(&accept_action_t::finalize, this->shared_from_this(), ph::_1)
        );
    }

private:
    void
    finalize(const std::error_code& ec) {
        // Prepare the internal socket object for consequential operations by moving its contents
        // to a heap-allocated object, which in turn might be attached to an engine.
        auto ptr = std::make_unique<socket_type>(std::move(socket));

        switch(ec.value()) {
        case 0:
            COCAINE_LOG_DEBUG(log, "accepted connection on fd {}", acceptor->native_handle());
            metrics.connections_accepted->fetch_add(1);

            try {
                context.engine().attach(std::move(ptr), prototype);
            } catch(const std::system_error& e) {
                COCAINE_LOG_ERROR(log, "unable to attach connection to engine: {}",
                    error::to_string(e));
                ptr = nullptr;
            }
            break;
        case asio::error::operation_aborted:
            return;
        default:
            COCAINE_LOG_ERROR(log, "unable to accept connection: [{}] {}", ec.value(),
                ec.message());
            metrics.connections_rejected->fetch_add(1);
            break;
        }

        // TODO: Find out if it's always a good idea to continue accepting connections no matter
        // what.
        // For example, destroying a socket from outside this thread will trigger weird stuff on
        // Linux.
        run();
    }
};

// Actor

template<typename Protocol>
actor_base<Protocol>::actor_base(context_t& context, std::unique_ptr<basic_dispatch_t> prototype) :
    actor_base(context, dispatch_ptr_t(prototype.release()))
{}

template<typename Protocol>
actor_base<Protocol>::actor_base(context_t& context, io::dispatch_ptr_t prototype) :
    m_context(context),
    m_log(context.log("core/asio", {{"service", prototype->name()}})),
    m_prototype(std::move(prototype))
{}

template<typename Protocol>
actor_base<Protocol>::~actor_base() = default;

template<typename Protocol>
bool
actor_base<Protocol>::is_active() const {
    return static_cast<bool>(*m_acceptor.synchronize());
}

template<typename Protocol>
io::dispatch_ptr_t
actor_base<Protocol>::prototype() const {
    return m_prototype;
}

template<typename Protocol>
void
actor_base<Protocol>::run() {
    m_acceptor.apply([this](std::shared_ptr<accept_action_t>& action) {
        auto endpoint = make_endpoint();

        std::unique_ptr<acceptor_type> acceptor;
        try {
            acceptor = m_context.expose<Protocol>(endpoint);
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to bind local endpoint {} for service: {}", endpoint, error::to_string(e));
            m_context.mapper().retain(m_prototype->name());
            throw;
        }

        std::error_code ec;
        COCAINE_LOG_INFO(m_log, "exposing service on local endpoint {}", acceptor->local_endpoint(ec));

        auto& loop = acceptor->get_io_service();
        action = std::make_shared<accept_action_t>(*this, std::move(acceptor));
        loop.post([=] {
            action->run();
        });
    });

    on_run();
}

template<typename Protocol>
void
actor_base<Protocol>::terminate() {
    m_acceptor.apply([this](std::shared_ptr<accept_action_t>& action) {
        const auto endpoint = action->local_endpoint();

        COCAINE_LOG_INFO(m_log, "removing service from local endpoint {}", endpoint);

        action->cancel();
        action = nullptr;
    });

    on_terminate();
}

template<typename Protocol>
auto
actor_base<Protocol>::local_endpoint() const -> endpoint_type {
    return m_acceptor.apply([&](const std::shared_ptr<accept_action_t>& action) {
        if (action) {
            return action->local_endpoint();
        } else {
            throw std::system_error(std::make_error_code(std::errc::not_connected));
        }
    });
}

template class cocaine::actor_base<asio::ip::tcp>;
template class cocaine::actor_base<asio::local::stream_protocol>;

static
auto
prototype_from(std::unique_ptr<api::service_t> service) -> dispatch_ptr_t {
    // Aliasing the pointer to the service to point to the dispatch (sub-)object.
    basic_dispatch_t* prototype = &service->prototype();

    return dispatch_ptr_t(std::shared_ptr<api::service_t>(std::move(service)), prototype);
}

tcp_actor_t::tcp_actor_t(context_t& context, std::unique_ptr<io::basic_dispatch_t> prototype) :
    actor_base(context, std::move(prototype)),
    context(context)
{}

tcp_actor_t::tcp_actor_t(context_t& context, std::unique_ptr<api::service_t> service) :
    actor_base(context, prototype_from(std::move(service))),
    context(context)
{}

auto
tcp_actor_t::endpoints() const -> std::vector<endpoint_type> {
    try {
        auto local = local_endpoint();

        if(!local.address().is_unspecified()) {
            return std::vector<tcp::endpoint>{{local}};
        }

        // For unspecified bind addresses, actual address set has to be resolved first. In other
        // words, unspecified means every available and reachable address for the host.
        std::vector<endpoint_type> endpoints;

        const tcp::resolver::query::flags flags =
            tcp::resolver::query::address_configured |
            tcp::resolver::query::numeric_service;

        asio::io_service loop;
        tcp::resolver::iterator begin = tcp::resolver(loop).resolve(
            tcp::resolver::query(
                context.config().network().hostname(),
                std::to_string(local.port()),
                flags
            )
        );

        std::transform(
            begin,
            tcp::resolver::iterator(),
            std::back_inserter(endpoints),
            std::bind(&tcp::resolver::iterator::value_type::endpoint, ph::_1)
        );

        return endpoints;
    } catch(const std::system_error&) {
        return std::vector<endpoint_type>();
    }
}

auto
tcp_actor_t::make_endpoint() const -> endpoint_type {
    auto name = prototype()->name();
    const auto port = context.mapper().assign(name);

    try {
        auto addr = ip::address::from_string(context.config().network().endpoint());
        return tcp::endpoint{addr, port};
    } catch(const std::system_error& e) {
        context.mapper().retain(name);
        throw;
    }
}

auto
tcp_actor_t::on_terminate() -> void {
    // Mark this service's port as free.
    context.mapper().retain(prototype()->name());
}
