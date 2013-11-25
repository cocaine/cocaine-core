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

#include "cocaine/detail/actor.hpp"

#include "cocaine/api/service.hpp"

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/connector.hpp"
#include "cocaine/asio/reactor.hpp"

#include "cocaine/context.hpp"
#include "cocaine/dispatch.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#if defined(__linux__)
    #include <sys/prctl.h>
#endif

using namespace cocaine;

actor_t::actor_t(context_t& context, std::shared_ptr<io::reactor_t> reactor, std::unique_ptr<io::dispatch_t>&& prototype):
    m_context(context),
    m_log(new logging::log_t(context, prototype->name())),
    m_reactor(reactor),
    m_prototype(std::move(prototype))
{ }

actor_t::actor_t(context_t& context, std::shared_ptr<io::reactor_t> reactor, std::unique_ptr<api::service_t>&& service):
    m_context(context),
    m_log(new logging::log_t(context, service->prototype().name())),
    m_reactor(reactor)
{
    io::dispatch_t *const ptr = &service->prototype();

    m_prototype = std::shared_ptr<io::dispatch_t>(
        std::shared_ptr<api::service_t>(std::move(service)),
        ptr
    );
}

actor_t::~actor_t() {
    // Empty.
}

namespace {

struct named_runnable {
    void
    operator()() const {
#if defined(__linux__)
        if(name.size() < 16) {
            ::prctl(PR_SET_NAME, name.c_str());
        } else {
            ::prctl(PR_SET_NAME, name.substr(0, 16).data());
        }
#endif

        reactor.run();
    }

    const std::string name;
    io::reactor_t& reactor;
};

} // namespace

void
actor_t::run(std::vector<io::tcp::endpoint> endpoints) {
    BOOST_ASSERT(!m_thread);

    for(auto it = endpoints.cbegin(); it != endpoints.cend(); ++it) {
        m_connectors.emplace_back(
            *m_reactor,
            std::make_unique<io::acceptor<io::tcp>>(*it)
        );

        m_connectors.back().bind(std::bind(&actor_t::on_connect, this, std::placeholders::_1));
    }

    m_thread = std::make_unique<boost::thread>(named_runnable{m_prototype->name(), *m_reactor});
}

void
actor_t::terminate() {
    BOOST_ASSERT(m_thread);

    m_reactor->post(std::bind(&io::reactor_t::stop, m_reactor));

    m_thread->join();
    m_thread.reset();

    m_connectors.clear();
}

auto
actor_t::location() const -> std::vector<io::tcp::endpoint> {
    BOOST_ASSERT(!m_connectors.empty());

    std::vector<io::tcp::endpoint> endpoints;

    for(auto it = m_connectors.begin(); it != m_connectors.end(); ++it) {
        endpoints.push_back(it->endpoint());
    }

    return endpoints;
}

auto
actor_t::metadata() const -> metadata_t {
    const auto port = location().front().port();
    const auto endpoint = io::locator::resolve::endpoint_tuple_type(m_context.config.network.hostname, port);

    return metadata_t(
        endpoint,
        m_prototype->versions(),
        m_prototype->protocol()
    );
}

void
actor_t::on_connect(const std::shared_ptr<io::socket<io::tcp>>& socket) {
    COCAINE_LOG_DEBUG(m_log, "accepted a new client from '%s' on fd %d", socket->remote_endpoint(), socket->fd());

    // This won't attach the socket immediately, instead it will post a new action to the designated
    // unit's event loop queue. It could probably be done with some locking, but whatever.
    m_context.attach(socket, m_prototype);
}
