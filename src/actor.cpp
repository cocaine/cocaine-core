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
using namespace cocaine::io;

using namespace std::placeholders;

actor_t::actor_t(context_t& context, std::shared_ptr<reactor_t> reactor, std::unique_ptr<dispatch_t>&& prototype):
    m_context(context),
    m_log(new logging::log_t(context, prototype->name())),
    m_reactor(reactor),
    m_prototype(std::move(prototype))
{ }

actor_t::actor_t(context_t& context, std::shared_ptr<reactor_t> reactor, std::unique_ptr<api::service_t>&& service):
    m_context(context),
    m_log(new logging::log_t(context, service->prototype().name())),
    m_reactor(reactor)
{
    std::shared_ptr<api::service_t> enclosure(std::move(service));

    m_prototype = std::shared_ptr<dispatch_t>(
        enclosure,
       &enclosure->prototype()
    );
}

actor_t::~actor_t() {
    m_prototype.reset();

    for(auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        // Synchronously close the connections.
        it->second->revoke();
    }
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

        reactor->run();
    }

    const std::string name;
    const std::shared_ptr<reactor_t>& reactor;
};

} // namespace

void
actor_t::run(std::vector<tcp::endpoint> endpoints) {
    BOOST_ASSERT(!m_thread);

    for(auto it = endpoints.cbegin(); it != endpoints.cend(); ++it) {
        m_connectors.emplace_back(
            *m_reactor,
            std::make_unique<acceptor<tcp>>(*it)
        );

        m_connectors.back().bind(std::bind(&actor_t::on_connect, this, _1));
    }

    m_thread.reset(new std::thread(named_runnable {
        m_prototype->name(),
        m_reactor
    }));
}

void
actor_t::terminate() {
    BOOST_ASSERT(m_thread);

    m_reactor->post(std::bind(&reactor_t::stop, m_reactor));

    m_thread->join();
    m_thread.reset();

    m_connectors.clear();
}

std::vector<tcp::endpoint>
actor_t::location() const {
    BOOST_ASSERT(!m_connectors.empty());

    std::vector<tcp::endpoint> endpoints;

    for(auto it = m_connectors.begin(); it != m_connectors.end(); ++it) {
        endpoints.push_back(it->endpoint());
    }

    return endpoints;
}

auto
actor_t::metadata() const -> metadata_t {
    const auto port = location().front().port();
    const auto endpoint = locator::endpoint_tuple_type(m_context.config.network.hostname, port);

    return metadata_t(
        endpoint,
        m_prototype->versions(),
        m_prototype->protocol()
    );
}

auto
actor_t::counters() const -> counters_t {
    counters_t result;

    result.sessions = m_sessions.size();

    for(auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        std::lock_guard<std::mutex> guard(it->second->mutex);

        auto info = std::make_tuple(
            it->second->downstreams.size(),
            it->second->ptr->footprint()
        );

        result.footprints.insert({ it->second->ptr->remote_endpoint(), info });
    }

    return result;
}

void
actor_t::on_connect(const std::shared_ptr<io::socket<tcp>>& socket_) {
    const int fd = socket_->fd();

    BOOST_ASSERT(m_channels.find(fd) == m_channels.end());

    COCAINE_LOG_DEBUG(m_log, "accepted a new client from '%s' on fd %d", socket_->remote_endpoint(), fd);

    auto ptr = std::make_unique<channel<io::socket<tcp>>>(*m_reactor, socket_);

    ptr->rd->bind(
        std::bind(&actor_t::on_message, this, fd, _1),
        std::bind(&actor_t::on_failure, this, fd, _1)
    );

    ptr->wr->bind(
        std::bind(&actor_t::on_failure, this, fd, _1)
    );

    m_sessions[fd] = std::make_shared<session_t>(std::move(ptr), m_prototype);
}

void
actor_t::on_message(int fd, const message_t& message) {
    auto it = m_sessions.find(fd);

    BOOST_ASSERT(it != m_sessions.end());

    try {
        it->second->invoke(message);
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(m_log, "dropping client on fd %d: %s", fd, e.what());

        // NOTE: This destroys the connection but not necessarily the session itself, as it might be
        // still in use by shared upstreams even in other threads. In other words, this doesn't guarantee
        // that the session will be actually deleted, but it's fine, since the connection is closed.
        it->second->revoke();
        m_sessions.erase(it);
    }
}

void
actor_t::on_failure(int fd, const std::error_code& ec) {
    if(m_sessions.find(fd) == m_sessions.end()) {
        // TODO: COCAINE-75 fixes this via cancellation.
        // Check whether the connection actually exists, in case multiple errors were queued up in
        // the reactor and it was already dropped.
        return;
    } else if(ec) {
        COCAINE_LOG_ERROR(m_log, "client on fd %d has disappeared - [%d] %s", fd, ec.value(), ec.message());
    } else {
        COCAINE_LOG_DEBUG(m_log, "client on fd %d has disconnected", fd);
    }

    m_sessions[fd]->revoke();
    m_sessions.erase(fd);
}
