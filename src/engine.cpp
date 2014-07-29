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

#include "cocaine/detail/engine.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/detail/chamber.hpp"

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/session.hpp"

using namespace cocaine;

execution_unit_t::execution_unit_t(context_t& context, const std::string& name):
    m_log(context.log(name)),
    m_reactor(std::make_shared<io::reactor_t>()),
    m_chamber(std::make_unique<io::chamber_t>(name, m_reactor))
{ }

execution_unit_t::~execution_unit_t() {
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

    // Make sure that the fd wasn't reused before it was actually processed for disconnection.
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
        COCAINE_LOG_ERROR(m_log, "client has been forced to disconnect")(
            "fd", fd,
            "reason", e.what()
        );

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
        COCAINE_LOG_ERROR(m_log, "client has disconnected")(
            "fd", fd,
            "errno", error.value(),
            "reason", error.message()
        );
    } else {
        COCAINE_LOG_DEBUG(m_log, "client has disconnected")("fd", fd);
    }

    m_sessions[fd]->detach();
    m_sessions.erase(fd);
}
