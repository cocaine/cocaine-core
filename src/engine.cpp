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

#include "cocaine/detail/chamber.hpp"

#include "cocaine/rpc/asio/channel.hpp"
#include "cocaine/rpc/session.hpp"

#include <blackhole/scoped_attributes.hpp>

using namespace blackhole;

using namespace asio;
using namespace asio::ip;

using namespace cocaine;

execution_unit_t::execution_unit_t(context_t& context):
    m_asio(new io_service()),
    m_chamber(new io::chamber_t("core:asio", m_asio))
{
    m_log = context.log("core:asio", {
        attribute::make("engine", boost::lexical_cast<std::string>(m_chamber->thread_id()))
    });

    COCAINE_LOG_DEBUG(m_log, "engine started");
}

namespace {

struct detach_action_t {
    void
    operator()();

    // Never becomes a dangling reference.
    std::map<int, std::shared_ptr<session_t>>& sessions;
};

void
detach_action_t::operator()() {
    for(auto it = sessions.begin(); it != sessions.end(); ++it) {
        it->second->detach();
    }
}

} // namespace

execution_unit_t::~execution_unit_t() {
    if(!m_sessions.empty()) {
        COCAINE_LOG_DEBUG(m_log, "engine waiting for outstanding operations to complete");

        // Asynchronously close the connections.
        m_asio->post(detach_action_t{m_sessions});
    }

    // NOTE: This will block until all the outstanding operations are complete.
    m_chamber = nullptr;
}

void
execution_unit_t::attach(const std::shared_ptr<tcp::socket>& ptr, const io::dispatch_ptr_t& dispatch) {
    m_asio->dispatch(std::bind(&execution_unit_t::attach_impl, this, ptr, dispatch));
}

double
execution_unit_t::utilization() const {
    return m_chamber->load_avg1();
}

void
execution_unit_t::attach_impl(const std::shared_ptr<tcp::socket>& ptr, const io::dispatch_ptr_t& dispatch) {
    int socket;

    if((socket = ::dup(ptr->native_handle())) == -1) {
        std::error_code ec(errno, std::system_category());
        COCAINE_LOG_ERROR(m_log, "unable to clone client's socket - [%d] %s", ec.value(), ec.message());
        return;
    }

    std::shared_ptr<session_t> session;

    try {
        // Local endpoint address of the socket to be cloned.
        const auto protocol = ptr->local_endpoint().protocol();

        // Copy the socket into the new reactor.
        auto channel = std::make_unique<io::channel<tcp>>(std::make_unique<tcp::socket>(
           *m_asio,
            protocol,
            socket
        ));

        // Create the new inactive session.
        session = std::make_shared<session_t>(std::move(channel), dispatch);
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "client has disappeared while creating session: [%d] %s",
            e.code().value(), e.code().message());
        return;
    }

    session->signals.shutdown.connect(std::bind(&execution_unit_t::on_shutdown,
        this,
        std::placeholders::_1,
        socket
    ));

    // Register the new session with this engine.
    m_sessions[socket] = session;

    COCAINE_LOG_DEBUG(m_log, "attached client to engine with %.2f%% utilization", utilization() * 100)(
        "endpoint", session->remote_endpoint(),
        "service",  session->name()
    );

    // Start the message dispatching.
    session->pull();
}

void
execution_unit_t::on_shutdown(const std::error_code& ec, int socket) {
    auto it = m_sessions.find(socket);

    BOOST_ASSERT(ec && it != m_sessions.end());

    scoped_attributes_t attributes(*m_log, {
        attribute::make("endpoint", boost::lexical_cast<std::string>(it->second->remote_endpoint())),
        attribute::make("service",  it->second->name())
    });

    if(ec != asio::error::eof) {
        COCAINE_LOG_ERROR(m_log, "client has disconnected: [%d] %s", ec.value(), ec.message());
    } else {
        COCAINE_LOG_DEBUG(m_log, "client has disconnected");
    }

    it->second->detach();
    m_sessions.erase(it);
}
