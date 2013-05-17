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
#include "cocaine/asio/resolver.hpp"
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

    auto endpoint = io::udp::endpoint(context.config.network.group, 0);

    if(context.config.network.aggregate) {
        io::udp::endpoint bindpoint("0.0.0.0", 10054);

        m_sink.reset(new io::socket<io::udp>());

        if(::bind(m_sink->fd(), bindpoint.data(), bindpoint.size()) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to bind an announce socket");
        }

        COCAINE_LOG_INFO(m_log, "joining multicast group '%s' on '%s'", endpoint.address(), bindpoint);

        group_req request;

        std::memset(&request, 0, sizeof(request));

        request.gr_interface = 0;

        std::memcpy(&request.gr_group, endpoint.data(), endpoint.size());

        if(::setsockopt(m_sink->fd(), IPPROTO_IP, MCAST_JOIN_GROUP, &request, sizeof(request)) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to join a multicast group");
        }

        m_sink_watcher.reset(new ev::io(m_reactor.native()));
        m_sink_watcher->set<locator_t, &locator_t::on_announce_event>(this);
        m_sink_watcher->start(m_sink->fd(), ev::READ);
    }

    endpoint.port(10054);

    COCAINE_LOG_INFO(m_log, "announcing the node on '%s'", endpoint);

    // NOTE: Connect an UDP socket so that we could send announces via write() instead of sendto().
    m_announce.reset(new io::socket<io::udp>(endpoint));

    const int loop = 0;
    const int life = IP_DEFAULT_MULTICAST_TTL;

    // NOTE: I don't think these calls might fail at all.
    ::setsockopt(m_announce->fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    ::setsockopt(m_announce->fd(), IPPROTO_IP, IP_MULTICAST_TTL,  &life, sizeof(life));

    m_announce_timer.reset(new ev::timer(m_reactor.native()));
    m_announce_timer->set<locator_t, &locator_t::on_announce_timer>(this);
    m_announce_timer->start(0.0f, 5.0f);
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
        "publishing service '%s' on :%s",
        name,
        std::get<1>(service->endpoint())
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
    query(const std::unique_ptr<actor_t>& actor) {
        return std::make_tuple(
            actor->endpoint(),
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

    return query(it->second);
}

namespace {
    struct dump_to {
        template<class T>
        void
        operator()(const T& service) {
            target[service.first] = query(service.second);
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

void
locator_t::on_announce_event(ev::io&, int) {
    char buffer[1024];
    std::error_code ec;

    ssize_t size = m_sink->read(buffer, sizeof(buffer), ec);

    if(size <= 0) {
        if(ec) {
            COCAINE_LOG_ERROR(m_log, "unable to receive an announce - [%d] %s", ec.value(), ec.message());
        }

        return;
    }

    msgpack::unpacked unpacked;
    msgpack::unpack(&unpacked, buffer, size);

    std::tuple<std::string, std::string> key;

    try {
        key = unpacked.get().as<std::tuple<std::string, std::string>>();
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(m_log, "unable to decode an announce");
        return;
    }

    if(m_remotes.find(key) == m_remotes.end()) {
        std::string hostname;
        std::string uuid;

        std::tie(hostname, uuid) = key;

        COCAINE_LOG_INFO(m_log, "discovered node '%s' on '%s'", uuid, hostname);

        std::shared_ptr<io::channel<io::socket<io::tcp>>> channel;

        try {
            channel = std::make_shared<io::channel<io::socket<io::tcp>>>(
                m_reactor,
                std::make_shared<io::socket<io::tcp>>(
                    io::resolver<io::tcp>::query(hostname, 10053)
                )
            );
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(m_log, "unable to connect to node '%s' - %s", hostname, e.what());
            return;
        }

        auto on_response = std::bind(&locator_t::on_response, this, key, _1);
        auto on_shutdown = std::bind(&locator_t::on_shutdown, this, key, _1);

        channel->wr->bind(on_shutdown);
        channel->rd->bind(on_response, on_shutdown);

        m_remotes[key] = channel;

        channel->wr->write<io::locator::dump>(0UL);
    }
}

void
locator_t::on_announce_timer(ev::timer&, int) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    packer << std::make_tuple(m_context.config.network.hostname, m_id.string());

    std::error_code ec;

    if(m_announce->write(buffer.data(), buffer.size(), ec) != static_cast<ssize_t>(buffer.size())) {
        if(ec) {
            COCAINE_LOG_ERROR(m_log, "unable to announce the node - [%d] %s", ec.value(), ec.message());
        } else {
            COCAINE_LOG_ERROR(m_log, "unable to announce the node - unexpected exception");
        }
    }
}

void
locator_t::on_response(const remote_map_t::key_type& key, const io::message_t& message) {
    std::string hostname;
    std::string uuid;

    std::tie(hostname, uuid) = key;

    switch(message.id()) {
        case io::event_traits<io::rpc::chunk>::id: {
            std::string chunk;
            dump_result_type dump;

            message.as<io::rpc::chunk>(chunk);

            msgpack::unpacked unpacked;
            msgpack::unpack(&unpacked, chunk.data(), chunk.size());

            unpacked.get() >> dump;

            COCAINE_LOG_INFO(m_log, "discovered %llu services on node '%s'", dump.size(), uuid);

            break;
        }

        case io::event_traits<io::rpc::error>::id: {
            break;
        }

        case io::event_traits<io::rpc::choke>::id: {
            break;
        }
    }
}

void
locator_t::on_shutdown(const remote_map_t::key_type& key, const std::error_code& ec) {
    COCAINE_LOG_INFO(
        m_log,
        "node '%s' has disconnected - [%d] %s",
        std::get<1>(key),
        ec.value(),
        ec.message()
    );

    m_remotes.erase(key);
}
