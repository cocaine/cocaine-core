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

#include "cocaine/rpc/session.hpp"

#if defined(__linux__)
    #include <sys/prctl.h>
#endif

using namespace cocaine;

// Execution unit

class actor_t::execution_unit_t {
    const std::unique_ptr<logging::log_t> m_log;
    const std::shared_ptr<io::dispatch_t> m_prototype;

    // Connections

    synchronized<
        std::map<int, std::shared_ptr<session_t>>
    > m_sessions;

    // I/O Reactor

    io::reactor_t m_reactor;
    boost::thread m_chamber;

public:
    execution_unit_t(context_t& context, const std::shared_ptr<io::dispatch_t>& prototype);
   ~execution_unit_t();

    void
    attach(const std::shared_ptr<io::socket<io::tcp>> socket);

    auto
    report() const -> actor_t::counters_t;

private:
    void
    on_connect(const std::shared_ptr<io::socket<io::tcp>> socket);

    void
    on_message(int fd, const io::message_t& message);

    void
    on_failure(int fd, const std::error_code& error);
};

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

actor_t::execution_unit_t::execution_unit_t(context_t& context, const std::shared_ptr<io::dispatch_t>& prototype):
    m_log(new logging::log_t(context, prototype->name())),
    m_prototype(prototype),
    m_chamber(named_runnable{m_prototype->name(), m_reactor})
{ }

actor_t::execution_unit_t::~execution_unit_t() {
    m_reactor.post(std::bind(&io::reactor_t::stop, &m_reactor));
    m_chamber.join();

    auto locked = m_sessions.synchronize();

    for(auto it = locked->cbegin(); it != locked->cend(); ++it) {
        // Synchronously close the connections.
        it->second->revoke();
    }

    locked->clear();
}

void
actor_t::execution_unit_t::attach(const std::shared_ptr<io::socket<io::tcp>> socket) {
    m_reactor.post(std::bind(&execution_unit_t::on_connect, this, socket));
}

auto
actor_t::execution_unit_t::report() const -> actor_t::counters_t {
    counters_t result;

    auto locked = m_sessions.synchronize();

    for(auto it = locked->begin(); it != locked->end(); ++it) {
        std::lock_guard<std::mutex> guard(it->second->mutex);

        auto info = std::make_tuple(
            it->second->downstreams.size(),
            it->second->ptr->footprint()
        );

        result.footprints.insert({it->second->ptr->remote_endpoint(), info});
    }

    result.sessions = locked->size();

    return result;
}

void
actor_t::execution_unit_t::on_connect(const std::shared_ptr<io::socket<io::tcp>> socket) {
    auto fd = socket->fd();

    BOOST_ASSERT(m_sessions->find(fd) == m_sessions->end());

    auto ptr = std::make_unique<io::channel<io::socket<io::tcp>>>(m_reactor, socket);

    using namespace std::placeholders;

    ptr->rd->bind(
        std::bind(&execution_unit_t::on_message, this, fd, _1),
        std::bind(&execution_unit_t::on_failure, this, fd, _1)
    );

    ptr->wr->bind(
        std::bind(&execution_unit_t::on_failure, this, fd, _1)
    );

    m_sessions->insert({fd, std::make_shared<session_t>(std::move(ptr), m_prototype)});
}

void
actor_t::execution_unit_t::on_message(int fd, const io::message_t& message) {
    auto it = m_sessions->find(fd);

    BOOST_ASSERT(it != m_sessions->end());

    try {
        it->second->invoke(message);
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(m_log, "client on fd %d has disconnected - %s", fd, e.what());

        // NOTE: This destroys the connection but not necessarily the session itself, as it might be
        // still in use by shared upstreams even in other threads. In other words, this doesn't guarantee
        // that the session will be actually deleted, but it's fine, since the connection is closed.
        it->second->revoke();
        m_sessions->erase(it);
    }
}

void
actor_t::execution_unit_t::on_failure(int fd, const std::error_code& error) {
    auto it = m_sessions->find(fd);

    if(it == m_sessions->end()) {
        // TODO: COCAINE-75 fixes this via cancellation.
        // Check whether the connection actually exists, in case multiple errors were queued up in
        // the reactor and it was already dropped.
        return;
    } else if(error) {
        COCAINE_LOG_ERROR(m_log, "client on fd %d has disconnected - [%d] %s", fd, error.value(), error.message());
    } else {
        COCAINE_LOG_DEBUG(m_log, "client on fd %d has disconnected", fd);
    }

    it->second->revoke();
    m_sessions->erase(fd);
}

// Actor

actor_t::actor_t(context_t& context, std::shared_ptr<io::reactor_t> reactor, std::unique_ptr<io::dispatch_t>&& prototype):
    m_context(context),
    m_log(new logging::log_t(context, prototype->name())),
    m_reactor(reactor),
    m_prototype(std::move(prototype))
{ }

actor_t::actor_t(context_t& context, std::shared_ptr<io::reactor_t> reactor, std::unique_ptr<api::service_t>&& service):
    m_context(context),
    m_log(new logging::log_t(context, service->prototype().name())),
    m_reactor(reactor),
    m_prototype(std::shared_ptr<api::service_t>(std::move(service)), &service->prototype())
{ }

actor_t::~actor_t() {
    // Empty.
}

void
actor_t::run(std::vector<io::tcp::endpoint> endpoints, unsigned int units) {
    BOOST_ASSERT(!m_thread);

    for(auto it = endpoints.cbegin(); it != endpoints.cend(); ++it) {
        m_connectors.emplace_back(
            *m_reactor,
            std::make_unique<io::acceptor<io::tcp>>(*it)
        );

        m_connectors.back().bind(std::bind(&actor_t::on_connect, this, std::placeholders::_1));
    }

    while(--units) {
        m_pool.emplace_back(std::make_unique<execution_unit_t>(m_context, m_prototype));
    }

    m_thread = std::make_unique<boost::thread>(named_runnable {
        m_prototype->name(),
        *m_reactor
    });
}

void
actor_t::terminate() {
    BOOST_ASSERT(m_thread);

    m_reactor->post(std::bind(&io::reactor_t::stop, m_reactor));

    m_thread->join();
    m_thread.reset();

    m_connectors.clear();

    // Synchronously terminate the execution units.
    m_pool.clear();
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
    const auto endpoint = io::locator::endpoint_tuple_type(m_context.config.network.hostname, port);

    return metadata_t(
        endpoint,
        m_prototype->versions(),
        m_prototype->protocol()
    );
}

auto
actor_t::counters() const -> counters_t {
    counters_t result;

    for(auto it = m_pool.begin(); it != m_pool.end(); ++it) {
        auto report = (*it)->report();

        result.sessions += report.sessions;
        result.footprints.insert(report.footprints.begin(), report.footprints.end());
    }

    return result;
}

void
actor_t::on_connect(const std::shared_ptr<io::socket<io::tcp>>& socket) {
    COCAINE_LOG_DEBUG(m_log, "accepted a new client from '%s' on fd %d", socket->remote_endpoint(), socket->fd());

    // This won't attach the socket immediately, instead it will post a new action to the unit's
    // event loop queue. It could probably be done with some locking, but whatever. I guess that the
    // socket file descriptor as a source of entropy is good enough for fair balancing.
    m_pool[socket->fd() % m_pool.size()]->attach(socket);
}
