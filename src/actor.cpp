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

#include "cocaine/detail/chamber.hpp"
#include "cocaine/detail/engine.hpp"

#include "cocaine/rpc/dispatch.hpp"

#include <blackhole/scoped_attributes.hpp>

using namespace blackhole;

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace cocaine;

// Actor internals

class actor_t::accept_action_t:
    public std::enable_shared_from_this<accept_action_t>
{
    actor_t* impl;

    // Holds new connection information, populated by Boost ASIO.
    tcp::socket   peer;
    tcp::endpoint endpoint;

public:
    accept_action_t(actor_t* impl_):
        impl(impl_),
        peer(*impl_->m_asio)
    { }

    void
    operator()();

private:
    void
    finalize(const boost::system::error_code& ec);
};

void
actor_t::accept_action_t::operator()() {
    impl->m_acceptor->async_accept(peer, endpoint, std::bind(&accept_action_t::finalize,
        shared_from_this(),
        std::placeholders::_1
    ));
}

void
actor_t::accept_action_t::finalize(const boost::system::error_code& ec) {
    scoped_attributes_t attributes(*impl->m_log, {
        attribute::make("service", impl->m_prototype->name())
    });

    if(ec) {
        if(ec == boost::asio::error::operation_aborted) {
            return;
        }

        COCAINE_LOG_ERROR(impl->m_log, "unable to accept client connection: [%d] %s",
            ec.value(), ec.message()
        );
    } else {
        execution_unit_t& engine = impl->m_context.engine();

        // This won't attach the socket immediately, instead it will post a new action to the
        // designated unit's event loop queue.
        engine.attach(std::make_shared<tcp::socket>(std::move(peer)), impl->m_prototype);
    }

    operator()();
}

// Actor

actor_t::actor_t(context_t& context, const std::shared_ptr<io_service>& asio,
                 std::unique_ptr<io::basic_dispatch_t> prototype)
:
    m_context(context),
    m_log(context.log("core:asio")),
    m_asio(asio),
    m_prototype(std::move(prototype))
{ }

actor_t::actor_t(context_t& context, const std::shared_ptr<io_service>& asio,
                 std::unique_ptr<api::service_t> service)
:
    m_context(context),
    m_log(context.log("core:asio")),
    m_asio(asio)
{
    const io::basic_dispatch_t* prototype = &service->prototype();

    // Aliasing the pointer to the service to point to the dispatch (sub-)object.
    m_prototype = io::dispatch_ptr_t(
        std::shared_ptr<api::service_t>(std::move(service)),
        prototype
    );
}

actor_t::~actor_t() {
    // Empty.
}

std::vector<tcp::endpoint>
actor_t::endpoints() const {
    if(!is_active()) {
        return std::vector<tcp::endpoint>();
    }

    if(!m_acceptor->local_endpoint().address().is_unspecified()) {
        return std::vector<tcp::endpoint>({m_acceptor->local_endpoint()});
    }

    tcp::resolver::query::flags flags =
        tcp::resolver::query::numeric_service
      | tcp::resolver::query::address_configured;

    tcp::resolver::iterator begin, end;

    try {
        begin = tcp::resolver(*m_asio).resolve(tcp::resolver::query(
            m_context.config.network.hostname, std::to_string(m_acceptor->local_endpoint().port()),
            flags
        ));
    } catch(const boost::system::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to determine local endpoints: [%d] %s",
            e.code().value(), e.code().message()
        )("service", m_prototype->name());
    }

    std::vector<tcp::endpoint> endpoints;

    std::transform(begin, end, std::back_inserter(endpoints), std::bind(
       &tcp::resolver::iterator::value_type::endpoint,
        std::placeholders::_1
    ));

    return endpoints;
}

bool
actor_t::is_active() const {
    return m_chamber && m_acceptor;
}

const io::basic_dispatch_t&
actor_t::prototype() const {
    return *m_prototype;
}

void
actor_t::run() {
    BOOST_ASSERT(!m_chamber);

    m_acceptor = std::make_unique<tcp::acceptor>(*m_asio, tcp::endpoint {
        m_context.config.network.endpoint,
        m_context.mapper.assign(m_prototype->name())
    });

    m_asio->post(std::bind(&accept_action_t::operator(),
        std::make_shared<accept_action_t>(this)
    ));

    // The post() above won't be executed until this thread is started.
    m_chamber = std::make_unique<io::chamber_t>(m_prototype->name(), m_asio);

    COCAINE_LOG_DEBUG(m_log, "service has been exposed on %s", m_acceptor->local_endpoint())(
        "service", m_prototype->name()
    );
}

void
actor_t::terminate() {
    BOOST_ASSERT(m_chamber);

    // Do not wait for the service to finish all its stuff (like timers, etc). Graceful termination
    // happens only in engine chambers, because that's where client connections are being handled.
    m_asio->stop();

    // Save the endpoint, as it won't be available after close().
    const auto endpoint = m_acceptor->local_endpoint();

    // To avoid remote clients hanging there waiting for accept(), while the service is actually on
    // its way to termination, close the socket.
    m_acceptor->close();
    m_context.mapper.retain(m_prototype->name(), endpoint.port());

    // Does not block, unlike the one in execution_unit_t's destructors.
    m_chamber  = nullptr;
    m_acceptor = nullptr;

    COCAINE_LOG_DEBUG(m_log, "service has been removed from %s", endpoint)(
        "service", m_prototype->name()
    );
}
