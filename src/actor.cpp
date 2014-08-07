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

#include "cocaine/detail/actor.hpp"

#include "cocaine/api/service.hpp"

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/connector.hpp"
#include "cocaine/asio/reactor.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/detail/chamber.hpp"

#include "cocaine/rpc/dispatch.hpp"

using namespace cocaine;

actor_t::actor_t(context_t& context, std::shared_ptr<io::reactor_t> reactor, std::unique_ptr<const io::basic_dispatch_t> prototype):
    m_context(context),
    m_log(context.log(prototype->name())),
    m_reactor(reactor)
{
    m_prototype = std::shared_ptr<const io::basic_dispatch_t>(
        std::move(prototype)
    );
}

actor_t::actor_t(context_t& context, std::shared_ptr<io::reactor_t> reactor, std::unique_ptr<const api::service_t> service):
    m_context(context),
    m_log(context.log(service->prototype().name())),
    m_reactor(reactor)
{
    const io::basic_dispatch_t* prototype = &service->prototype();

    // Aliasing the pointer to the service to point to the dispatch (sub-)object.
    m_prototype = std::shared_ptr<const io::basic_dispatch_t>(
        std::shared_ptr<const api::service_t>(std::move(service)),
        prototype
    );
}

actor_t::~actor_t() {
    // Empty.
}

void
actor_t::run(std::vector<io::tcp::endpoint> endpoints) {
    BOOST_ASSERT(!m_chamber);

    for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        m_connectors.emplace_back(
            *m_reactor,
            std::make_unique<io::acceptor<io::tcp>>(*it)
        );

        m_connectors.back().bind(std::bind(&actor_t::on_connect, this, std::placeholders::_1));
    }

    m_chamber = std::make_unique<io::chamber_t>(m_prototype->name(), m_reactor);
}

void
actor_t::terminate() {
    BOOST_ASSERT(m_chamber);

    m_chamber.reset();
    m_connectors.clear();
}

auto
actor_t::location() const -> std::vector<io::tcp::endpoint> {
    std::vector<io::tcp::endpoint> endpoints;

    for(auto it = m_connectors.begin(); it != m_connectors.end(); ++it) {
        endpoints.push_back(it->endpoint());
    }

    return endpoints;
}

auto
actor_t::metadata() const -> metadata_t {
    const auto port = location().front().port();
    const auto endpoint = io::locator::endpoint_tuple_type(m_context.config.network.hostname, port);

    return metadata_t(endpoint, m_prototype->versions(), m_prototype->protocol());
}

void
actor_t::on_connect(const std::shared_ptr<io::socket<io::tcp>>& socket) {
    COCAINE_LOG_DEBUG(m_log, "accepted a new client")(
        "endpoint", socket->remote_endpoint(),
        "fd", socket->fd()
    );

    // This won't attach the socket immediately, instead it will post a new action to the designated
    // unit's event loop queue. It could probably be done with some locking, but whatever.
    m_context.attach(socket, m_prototype);
}
