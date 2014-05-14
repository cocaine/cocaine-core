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

#include "cocaine/detail/engine.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/session.hpp"

#if defined(__linux__)
    #include <sys/prctl.h>
#endif

using namespace cocaine;

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
    const std::unique_ptr<io::reactor_t>& reactor;
};

} // namespace

execution_unit_t::execution_unit_t(context_t& context, const std::string& name):
    m_log(new logging::log_t(context, name)),
    m_reactor(std::make_unique<io::reactor_t>()),
    m_chamber(std::make_unique<boost::thread>(named_runnable{name, m_reactor}))
{ }

execution_unit_t::~execution_unit_t() {
    m_reactor->post(std::bind(&io::reactor_t::stop, m_reactor.get()));

    m_chamber->join();
    m_chamber.reset();

    for(auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        // Synchronously close the connections.
        it->second->detach();
    }

    m_sessions.clear();
}

void
execution_unit_t::attach(const std::shared_ptr<io::socket<io::tcp>>& socket, const std::shared_ptr<io::basic_dispatch_t>& dispatch) {
    m_reactor->post(std::bind(&execution_unit_t::on_connect, this, socket, dispatch));
}

void
execution_unit_t::on_connect(const std::shared_ptr<io::socket<io::tcp>>& socket, const std::shared_ptr<io::basic_dispatch_t>& dispatch) {
    auto fd = socket->fd();

    BOOST_ASSERT(!m_sessions.count(fd));

    auto ptr = std::make_unique<io::channel<io::socket<io::tcp>>>(*m_reactor, socket);

    using namespace std::placeholders;

    ptr->rd->bind(
        std::bind(&execution_unit_t::on_message, this, fd, _1),
        std::bind(&execution_unit_t::on_failure, this, fd, _1)
    );

    ptr->wr->bind(
        std::bind(&execution_unit_t::on_failure, this, fd, _1)
    );

    m_sessions[fd] = std::make_shared<session_t>(std::move(ptr), dispatch);
}

void
execution_unit_t::on_message(int fd, const io::message_t& message) {
    auto it = m_sessions.find(fd);

    BOOST_ASSERT(it != m_sessions.end());

    try {
        it->second->invoke(message);
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(m_log, "client on fd %d has been forced to disconnect - %s", fd, e.what());

        // NOTE: This destroys the connection but not necessarily the session itself, as it might be
        // still in use by shared upstreams even in other threads. In other words, this doesn't guarantee
        // that the session will be actually deleted, but it's fine, since the connection is closed.
        it->second->detach();
        m_sessions.erase(it);
    }
}

void
execution_unit_t::on_failure(int fd, const std::error_code& error) {
    if(!m_sessions.count(fd)) {
        // TODO: COCAINE-75 fixes this via cancellation.
        // Check whether the connection actually exists, in case multiple errors were queued up in
        // the reactor and it was already dropped.
        return;
    } else if(error) {
        COCAINE_LOG_ERROR(m_log, "client on fd %d has disconnected - [%d] %s", fd, error.value(), error.message());
    } else {
        COCAINE_LOG_DEBUG(m_log, "client on fd %d has disconnected", fd);
    }

    m_sessions[fd]->detach();
    m_sessions.erase(fd);
}
