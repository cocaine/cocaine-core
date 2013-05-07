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
#include "cocaine/asio/tcp.hpp"
#include "cocaine/asio/udp.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

using namespace cocaine;
using namespace std::placeholders;

locator_t::locator_t(context_t& context, io::reactor_t& reactor):
    dispatch_t(context, "service/locator"),
    m_context(context),
    m_log(new logging::log_t(context, "service/locator")),
    m_reactor(reactor)
{
    on<io::locator::resolve>("resolve", std::bind(&locator_t::resolve, this, _1));
    on<io::locator::dump>("dump", std::bind(&locator_t::dump, this));

    if(context.config.network.group.empty()) {
        return;
    }

    auto endpoint = io::udp::endpoint(context.config.network.group, 10054);

    // TODO: Make a bound-connect constructor.
    m_announce.reset(new io::socket<io::udp>());

    if(context.config.network.aggregate) {
        const int loop = 0;
        const int ttl  = IP_DEFAULT_MULTICAST_TTL;

        // NOTE: I don't think these calls might fail at all.
        ::setsockopt(m_announce->fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
        ::setsockopt(m_announce->fd(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

        group_req request;

        std::memset(&request, 0, sizeof(request));

        request.gr_interface = 0;

        io::udp::endpoint local("0.0.0.0", 10054);

        COCAINE_LOG_INFO(m_log, "joining multicast group '%s' on '%s'", endpoint.address(), local);

        if(::bind(m_announce->fd(), local.data(), local.size()) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to bind an announce socket");
        }

        std::memcpy(&request.gr_group, endpoint.data(), endpoint.size());

        if(::setsockopt(m_announce->fd(), IPPROTO_IP, MCAST_JOIN_GROUP, &request, sizeof(request)) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to join a multicast group");
        }

        m_announce_watcher.reset(new ev::io(m_reactor.native()));
        m_announce_watcher->set<locator_t, &locator_t::on_announce_event>(this);
        m_announce_watcher->start(m_announce->fd(), ev::READ);
    } else {
        COCAINE_LOG_INFO(m_log, "announcing the node on '%s'", endpoint);

        // NOTE: Connect an UDP socket so that we could send announces via write() instead of sendto().
        if(::connect(m_announce->fd(), endpoint.data(), endpoint.size()) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to connect a socket");
        }

        m_announce_timer.reset(new ev::timer(m_reactor.native()));
        m_announce_timer->set<locator_t, &locator_t::on_announce_timer>(this);
        m_announce_timer->start(0.0f, 5.0f);
    }
}

locator_t::~locator_t() {
    m_remotes.clear();

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
        operator()(const T& service) const {
            return name == service.first;
        }

        const std::string name;
    };

    inline
    resolve_result_type
    define(const std::unique_ptr<actor_t>& actor) {
        return std::make_tuple(
            actor->endpoint().tuple(),
            1u,
            actor->dispatch().describe()
        );
    }
}

resolve_result_type
locator_t::resolve(const std::string& name) const {
    auto it = std::find_if(m_services.begin(), m_services.end(), match {
        name
    });

    if(it == m_services.end()) {
        throw cocaine::error_t("the specified service is not available");
    }

    return define(it->second);
}

namespace {
    struct dump_to {
        template<class T>
        void
        operator()(const T& service) {
            target[service.first] = define(service.second);
        }

        dump_result_type& target;
    };
}

dump_result_type
locator_t::dump() const {
    dump_result_type result;

    std::for_each(m_services.begin(), m_services.end(), dump_to {
        result
    });

    return result;
}

namespace {
    struct ignore {
        void
        operator()(const std::error_code& /* ec */) const {
            // Do nothing.
        }
    };
}

void
locator_t::on_announce_event(ev::io&, int) {
    char hostname[1024];
    std::error_code ec;

    ssize_t size = m_announce->read(hostname, sizeof(hostname), ec);

    if(size <= 0) {
        if(ec) {
            COCAINE_LOG_ERROR(m_log, "unable to receive the announce - [%d] %s", ec.value(), ec.message());
        }

        return;
    }

    if(m_remotes.find(hostname) == m_remotes.end()) {
        COCAINE_LOG_INFO(m_log, "discovered a new node at '%s'", std::string(hostname, size));

        auto channel = std::make_shared<io::channel<io::socket<io::tcp>>>(
            m_reactor,
            std::make_shared<io::socket<io::tcp>>(io::tcp::endpoint(hostname, 10053))
        );

        channel->wr->bind(ignore());
        channel->rd->bind(std::bind(&locator_t::on_response, this, _1), ignore());

        m_remotes[hostname] = channel;

        channel->wr->write<io::locator::dump>(0UL);
    }
}

void
locator_t::on_announce_timer(ev::timer&, int) {
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

void
locator_t::on_response(const io::message_t& message) {
    switch(message.id()) {
        case io::event_traits<io::rpc::chunk>::id: {
            std::string chunk;

            message.as<io::rpc::chunk>(chunk);

            msgpack::unpacked unpacked;
            msgpack::unpack(&unpacked, chunk.data(), chunk.size());

            COCAINE_LOG_INFO(m_log, "discovered remote services: %s", unpacked.get());
        }

        case io::event_traits<io::rpc::error>::id: {
            break;
        }

        case io::event_traits<io::rpc::choke>::id: {
            break;
        }
    }
}
