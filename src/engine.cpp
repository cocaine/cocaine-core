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

#include "cocaine/engine.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/rpc/asio/transport.hpp"
#include "cocaine/rpc/basic_dispatch.hpp"
#include "cocaine/rpc/session.hpp"

#include <blackhole/logger.hpp>
#include <blackhole/wrapper.hpp>

#include <boost/lexical_cast.hpp>

#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/local/stream_protocol.hpp>

#include "chamber.hpp"

using namespace cocaine;
using namespace cocaine::io;

using namespace asio;

class execution_unit_t::gc_action_t:
    public std::enable_shared_from_this<gc_action_t>
{
    execution_unit_t *const parent;
    const boost::posix_time::seconds repeat;

public:
    template<class Interval>
    gc_action_t(execution_unit_t *const parent_, Interval repeat_):
        parent(parent_),
        repeat(repeat_)
    { }

    void
    operator()();

private:
    void
    finalize(const std::error_code& ec);
};

void
execution_unit_t::gc_action_t::operator()() {
    if(!parent->m_cron) {
        return;
    }

    parent->m_cron->expires_from_now(repeat);

    parent->m_cron->async_wait(std::bind(&gc_action_t::finalize,
        shared_from_this(),
        std::placeholders::_1
    ));
}

void
execution_unit_t::gc_action_t::finalize(const std::error_code& ec) {
    if(ec == asio::error::operation_aborted) {
        return;
    }

    size_t recycled = 0;

    for(auto it = parent->m_sessions.begin(); it != parent->m_sessions.end();) {
        if(!it->second->memory_pressure()) {
            recycled++;
            it = parent->m_sessions.erase(it);
            continue;
        }

        ++it;
    }

    if(recycled) {
        COCAINE_LOG_DEBUG(parent->m_log, "recycled {:d} session(s)", recycled);
    }

    operator()();
}

execution_unit_t::execution_unit_t(context_t& context):
    m_asio(new io_service()),
    m_chamber(new chamber_t("core/asio", m_asio)),
    m_log(context.log("core/asio", {{"engine", m_chamber->thread_id()}})),
    m_metrics(context.metrics_hub()),
    m_cron(new asio::deadline_timer(*m_asio))
{
    m_asio->post(std::bind(&gc_action_t::operator(),
        std::make_shared<gc_action_t>(this, boost::posix_time::seconds(kCollectionInterval))
    ));

    COCAINE_LOG_DEBUG(m_log, "engine started");
}

execution_unit_t::~execution_unit_t() {
    m_asio->post([this] {
        COCAINE_LOG_DEBUG(m_log, "stopping engine");

        for(auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            // Close the connections.
            it->second->detach(std::error_code());
        }

        // NOTE: It's okay to destroy deadline timer here, because garbage collector always performs
        // existence check for timer.
        m_cron.reset();
    });

    // NOTE: This will block until all the outstanding operations are complete.
    m_chamber = nullptr;
}

template<class Socket>
std::shared_ptr<session<typename Socket::protocol_type>>
execution_unit_t::attach(std::unique_ptr<Socket> ptr, const dispatch_ptr_t& dispatch) {
    typedef Socket socket_type;
    typedef typename socket_type::protocol_type protocol_type;
    typedef session<protocol_type> session_type;

    int fd;

    if((fd = ::dup(ptr->native_handle())) == -1) {
        throw std::system_error(errno, std::system_category(), "unable to clone client's socket");
    }

    std::shared_ptr<session_type> session_;

    try {
        // Local endpoint address of the socket to be cloned.
        const auto endpoint = ptr->local_endpoint();

        // Copy the socket into the new reactor.
        auto transport = std::make_unique<io::transport<protocol_type>>(
            std::make_unique<socket_type>(*m_asio, endpoint.protocol(), fd)
        );

        std::string remote_endpoint;

        if(std::is_same<protocol_type, ip::tcp>::value) {
            // Disable Nagle's algorithm, since most of the service clients do not send or receive
            // more than a couple of kilobytes of data.
            transport->socket->set_option(ip::tcp::no_delay(true));

            // Enabling keepalive for TCP socket is required to avoid weird IPVS behavior on erasing
            // a table record, which lead to infinite socket consuming and making us suffer from fd
            // leakage.
            // NOTE: There is another solution: with reading `null_buffers` every N seconds we can
            // check an error code received.
            transport->socket->set_option(asio::socket_base::keep_alive(true));
            remote_endpoint = boost::lexical_cast<std::string>(ptr->remote_endpoint());
        } else if(std::is_same<protocol_type, local::stream_protocol>::value) {
            remote_endpoint = boost::lexical_cast<std::string>(endpoint);
        } else {
            remote_endpoint = "<unknown>";
        }

        std::unique_ptr<logging::logger_t> log(new blackhole::wrapper_t(*m_log, {
            {"endpoint", remote_endpoint                       },
            {"service",  dispatch ? dispatch->name() : "<none>"}
        }));

        COCAINE_LOG_DEBUG(log, "attached connection to engine, load: {:.2f}%", utilization() * 100);

        // Create a new inactive session.
        session_ = std::make_shared<session_type>(std::move(log), m_metrics, std::move(transport), dispatch);
        // Start pulling right now to prevent race when session is detached before pull
        session_->pull();

        if (dispatch) {
            dispatch->attached(session_);
        }
    } catch(const std::system_error& e) {
        throw std::system_error(e.code(), "client has disappeared while creating session");
    }

    m_asio->dispatch([=]() mutable {
        m_sessions[fd] = std::move(session_);
    });

    return session_;
}

double
execution_unit_t::utilization() const {
    return m_chamber->load_avg1();
}

template
std::shared_ptr<session<ip::tcp>>
execution_unit_t::attach(std::unique_ptr<ip::tcp::socket>, const dispatch_ptr_t&);

template
std::shared_ptr<session<local::stream_protocol>>
execution_unit_t::attach(std::unique_ptr<local::stream_protocol::socket>, const dispatch_ptr_t&);
