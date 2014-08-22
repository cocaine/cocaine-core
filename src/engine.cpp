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

#include "cocaine/rpc/asio/channel.hpp"
#include "cocaine/rpc/session.hpp"

#include <blackhole/scoped_attributes.hpp>

using namespace blackhole;

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace cocaine;

execution_unit_t::execution_unit_t(context_t& context):
    m_log(context.log("cocaine/io-pool")),
    m_asio(new io_service()),
    m_chamber(new io::chamber_t("cocaine/io-pool", m_asio))
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
execution_unit_t::attach(const std::shared_ptr<tcp::socket>& ptr, const std::shared_ptr<const io::basic_dispatch_t>& dispatch) {
    m_asio->post(std::bind(&execution_unit_t::attach_impl, this, ptr, dispatch));
}

void
execution_unit_t::detach(int fd) {
    m_sessions[fd]->detach();
    m_sessions.erase(fd);
}

void
execution_unit_t::attach_impl(const std::shared_ptr<tcp::socket>& ptr, const std::shared_ptr<const io::basic_dispatch_t>& dispatch) {
    auto fd = ::dup(ptr->native_handle());

    // Make sure that the fd wasn't reused before it was actually processed for disconnection.
    BOOST_ASSERT(!m_sessions.count(fd));

    // Copy the socket into the new reactor.
    auto channel = std::make_unique<io::channel<tcp>>(std::make_unique<tcp::socket>(
       *m_asio,
        ptr->local_endpoint().protocol(),
        fd
    ));

    using namespace std::placeholders;

    m_sessions[fd] = std::make_shared<session_t>(std::move(channel), dispatch);
    m_sessions[fd]->signals.shutdown.connect(std::bind(&execution_unit_t::on_session_shutdown, this, _1, fd));

    // Start the message dispatching.
    m_sessions[fd]->pull();
}

void
execution_unit_t::on_session_shutdown(const boost::system::error_code& ec, int fd) {
    auto it = m_sessions.find(fd);

    BOOST_ASSERT(ec && it != m_sessions.end());

    scoped_attributes_t attributes(*m_log, {
        attribute::make("endpoint", boost::lexical_cast<std::string>(it->second->remote_endpoint())),
        attribute::make("service", it->second->name())
    });

    if(ec != boost::asio::error::eof) {
        COCAINE_LOG_ERROR(m_log, "client has disconnected: [%d] %s", ec.value(), ec.message());
    } else {
        COCAINE_LOG_DEBUG(m_log, "client has disconnected");
    }

    detach(fd);
}
