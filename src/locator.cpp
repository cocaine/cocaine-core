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

#include "cocaine/detail/locator.hpp"

#include "cocaine/asio/reactor.hpp"
#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/udp.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/detail/actor.hpp"

using namespace cocaine;
using namespace std::placeholders;

locator_t::locator_t(context_t& context, io::reactor_t& reactor):
    dispatch_t(context, "service/locator"),
    m_context(context),
    m_log(new logging::log_t(context, "service/locator"))
{
    on<io::locator::resolve>("resolve", std::bind(&locator_t::resolve, this, _1));

    if(!context.config.network.group.empty()) {
        auto endpoint = io::udp::endpoint(context.config.network.group, 10053);

        COCAINE_LOG_INFO(m_log, "announcing the node on '%s'", endpoint);

        // NOTE: Connect an UDP socket so that we could send announces via write() instead of sendto().
        m_announce.reset(new io::socket<io::udp>(endpoint));

        m_announce_timer.reset(new ev::timer(reactor.native()));
        m_announce_timer->set<locator_t, &locator_t::on_announce>(this);
        m_announce_timer->start(0.0f, 5.0f);
    }
}

locator_t::~locator_t() {
    for(service_list_t::reverse_iterator it = m_services.rbegin();
        it != m_services.rend();
        ++it)
    {
        COCAINE_LOG_INFO(m_log, "stopping service '%s'", it->first);

        // Terminate the service's thread.
        it->second->terminate();
    }

    m_services.clear();
}

void
locator_t::attach(const std::string& name, std::unique_ptr<actor_t>&& service) {
    COCAINE_LOG_INFO(
        m_log,
        "publishing service '%s' on '%s'",
        name,
        service->endpoint()
    );

    // Start the service's thread.
    service->run();

    m_services.emplace_back(
        name,
        std::move(service)
    );
}

namespace {
    struct match {
        template<class T>
        bool
        operator()(const T& service) {
            return name == service.first;
        }

        const std::string name;
    };
}

auto
locator_t::resolve(const std::string& name) const
    -> tuple::fold<io::locator::resolve::result_type>::type
{
    auto it = std::find_if(m_services.begin(), m_services.end(), match {
        name
    });

    if(it == m_services.end()) {
        throw cocaine::error_t("the specified service is not available");
    }

    return std::make_tuple(
        it->second->endpoint().tuple(),
        1u,
        it->second->dispatch().describe()
    );
}

void
locator_t::on_announce(ev::timer&, int) {
    std::error_code ec;

    const std::string& hostname = m_context.config.network.hostname;
    const size_t size = hostname.size();

    if(m_announce->write(hostname.data(), size, ec) != static_cast<ssize_t>(size)) {
        if(ec) {
            COCAINE_LOG_ERROR(m_log, "unable to announce the node - [%d] %s", ec.value(), ec.message());
        } else {
            COCAINE_LOG_ERROR(m_log, "unable to announce the node - unexpected exception");
        }
    }
}
