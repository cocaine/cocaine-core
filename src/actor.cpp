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

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/detail/chamber.hpp"

#include "cocaine/rpc/dispatch.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace cocaine;

// Actor internals

struct actor_t::connection_t {
    explicit
    connection_t(io_service& asio):
        socket(new tcp::socket(asio))
    { }

    std::shared_ptr<tcp::socket> socket;

    // Remote peer endpoint address.
    tcp::endpoint origin;
};

// Actor

actor_t::actor_t(context_t& context, std::shared_ptr<io_service> asio, std::unique_ptr<io::basic_dispatch_t> prototype):
    m_context(context),
    m_log(context.log(prototype->name())),
    m_asio(asio),
    m_prototype(std::move(prototype))
{ }

actor_t::actor_t(context_t& context, std::shared_ptr<io_service> asio, std::unique_ptr<api::service_t> service):
    m_context(context),
    m_log(context.log(service->prototype().name())),
    m_asio(asio)
{
    const io::basic_dispatch_t* prototype = &service->prototype();

    // Aliasing the pointer to the service to point to the dispatch (sub-)object.
    m_prototype = std::shared_ptr<const io::basic_dispatch_t>(
        std::shared_ptr<api::service_t>(std::move(service)),
        prototype
    );
}

actor_t::~actor_t() {
    // Empty.
}

void
actor_t::run(std::vector<tcp::endpoint> endpoints) {
    BOOST_ASSERT(!m_chamber);

    for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        COCAINE_LOG_DEBUG(m_log, "exposing service on [%s]", it->address())(
            "service", m_prototype->name()
        );

        m_connectors.emplace_back(*m_asio, *it);

        auto connection = std::make_shared<connection_t>(*m_asio);

        m_connectors.back().async_accept(*connection->socket, connection->origin,
            std::bind(&actor_t::accept_impl, this, std::placeholders::_1, connection)
        );
    }

    m_chamber = std::make_unique<io::chamber_t>(m_prototype->name(), m_asio);
}

void
actor_t::terminate() {
    BOOST_ASSERT(m_chamber);

    for(auto it = m_connectors.begin(); it != m_connectors.end(); ++it) {
        it->close();
    }

    m_connectors.clear();
    m_chamber.reset();
}

auto
actor_t::endpoints() const -> std::vector<tcp::endpoint> {
    if(!is_active()) {
        return std::vector<tcp::endpoint>();
    }

    tcp::resolver::iterator it, end;

    try {
        it = tcp::resolver(*m_asio).resolve(tcp::resolver::query(
            m_context.config.network.hostname,
            boost::lexical_cast<std::string>(m_connectors.front().local_endpoint().port())
        ));
    } catch(const boost::system::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to resolve local endpoints: [%d] %s", e.code().value(), e.code().message())(
            "service", m_prototype->name()
        );
    }

    return std::vector<tcp::endpoint>(it, end);
}

auto
actor_t::prototype() const -> const io::basic_dispatch_t& {
    return *m_prototype;
}

bool
actor_t::is_active() const {
    return m_chamber && !m_connectors.empty();
}

void
actor_t::accept_impl(const boost::system::error_code& ec, const std::shared_ptr<connection_t>& ptr) {
    if(ec) {
        if(ec == error::operation_aborted) {
            return;
        }

        COCAINE_LOG_ERROR(m_log, "unable to accept a new client connection: %s", ec)(
            "service", m_prototype->name()
        );
    } else {
        COCAINE_LOG_DEBUG(m_log, "accepted a new client connection")(
            "endpoint", ptr->origin,
            "service", m_prototype->name()
        );

        // This won't attach the socket immediately, instead it will post a new action to the designated
        // unit's event loop queue. It could probably be done with some locking, but whatever.
        m_context.attach(std::move(ptr->socket), m_prototype);
    }

    auto connection = std::make_shared<connection_t>(*m_asio);

    m_connectors.back().async_accept(*connection->socket, connection->origin,
        std::bind(&actor_t::accept_impl, this, std::placeholders::_1, connection)
    );
}
