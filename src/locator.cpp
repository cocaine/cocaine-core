/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#include "cocaine/api/gateway.hpp"

#include "cocaine/asio/reactor.hpp"
#include "cocaine/asio/resolver.hpp"
#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/tcp.hpp"
#include "cocaine/asio/timeout.hpp"
#include "cocaine/asio/udp.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/group.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/rpc/channel.hpp"

#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/tuple.hpp"

using namespace cocaine;
using namespace cocaine::io;

using namespace std::placeholders;

#include "routing.inl"

locator_t::locator_t(context_t& context, reactor_t& reactor):
    implementation<io::locator_tag>(context, "service/locator"),
    m_context(context),
    m_log(new logging::log_t(context, "service/locator")),
    m_reactor(reactor),
    m_router(new router_t(*m_log.get()))
{
    on<io::locator::resolve>(std::bind(&locator_t::resolve, this, _1));
    on<io::locator::refresh>(std::bind(&locator_t::refresh, this, _1));

    // NOTE: Slots for io::locator::synchronize and io::locator::reports actions are bound in
    // context_t::bootstrap(), as it's easier to implement them using context_t internals.

    COCAINE_LOG_INFO(m_log, "this node's id is '%s'", m_context.config.network.uuid);

    try {
        auto groups = api::storage(context, "core")->find("groups", std::vector<std::string>({
            "group",
            "active"
        }));

        for(auto it = groups.begin(); it != groups.end(); ++it) {
            m_router->add_group(*it, group_t(context, *it).to_map());
        }
    } catch(const storage_error_t& e) {
        throw cocaine::error_t("unable to initialize the routing groups - %s", e.what());
    }

    if(m_context.config.network.group) {
        connect();
    }
}

locator_t::~locator_t() {
    // Empty.
}

void
locator_t::connect() {
    using namespace boost::asio::ip;

    io::udp::endpoint endpoint = {
        address::from_string(m_context.config.network.group.get()),
        0
    };

    if(m_context.config.network.gateway) {
        const io::udp::endpoint bindpoint = { address::from_string("0.0.0.0"), 10054 };

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

        m_gateway = m_context.get<api::gateway_t>(
            m_context.config.network.gateway.get().type,
            m_context,
            "service/locator",
            m_context.config.network.gateway.get().args
        );
    }

    endpoint.port(10054);

    COCAINE_LOG_INFO(m_log, "announcing the node on '%s'", endpoint);

    // NOTE: Connect this UDP socket so that we could send announces via write() instead of sendto().
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

auto
locator_t::resolve(const std::string& name) const -> resolve_result_type {
    auto basename = m_router->select_service(name);
    auto provided = m_context.locate(basename);

    if(provided) {
        COCAINE_LOG_DEBUG(m_log, "providing '%s' using local node", name);

        // TODO: Might be a good idea to return an endpoint suitable for the interface which the
        // client used to connect to the Locator.
        return provided.get().metadata();
    }

    if(m_gateway) {
        return m_gateway->resolve(basename);
    } else {
        throw cocaine::error_t("the specified service is not available");
    }
}

auto
locator_t::refresh(const std::string& name) -> refresh_result_type {
    std::vector<std::string> groups;

    try {
        groups = api::storage(m_context, "core")->find("groups", std::vector<std::string>({
            "group",
            "active"
        }));
    } catch(const storage_error_t& e) {
        throw cocaine::error_t("unable to read the routing group list - %s", e.what());
    }

    if(std::find(groups.begin(), groups.end(), name) != groups.end()) {
        try {
            m_router->add_group(name, group_t(m_context, name).to_map());
        } catch(const storage_error_t& e) {
            throw cocaine::error_t("unable to read routing group '%s' - %s", name, e.what());
        }
    } else {
        m_router->remove_group(name);
    }
}

void
locator_t::on_announce_event(ev::io&, int) {
    char buffers[1024];
    std::error_code ec;

    const ssize_t size = m_sink->read(buffers, sizeof(buffers), ec);

    if(size <= 0) {
        if(ec) {
            COCAINE_LOG_ERROR(m_log, "unable to receive an announce - [%d] %s", ec.value(), ec.message());
        }

        return;
    }

    msgpack::unpacked unpacked;
    key_type key;

    try {
        msgpack::unpack(&unpacked, buffers, size);
        unpacked.get() >> key;
    } catch(const msgpack::unpack_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to decode an announce");
        return;
    } catch(const msgpack::type_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to decode an announce");
        return;
    }

    if(m_remotes.find(key) != m_remotes.end()) {
        return;
    }

    std::string uuid;
    std::string hostname;
    uint16_t    port;

    std::tie(uuid, hostname, port) = key;

    COCAINE_LOG_INFO(m_log, "discovered node '%s' on '%s:%d'", uuid, hostname, port);

    std::vector<io::tcp::endpoint> endpoints;

    try {
        endpoints = io::resolver<io::tcp>::query(hostname, port);
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to resolve node '%s' endpoints - [%d] %s", uuid, e.code().value(),
            e.code().message());
        return;
    }

    std::shared_ptr<io::channel<io::socket<io::tcp>>> channel;

    for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        try {
            channel = std::make_shared<io::channel<io::socket<io::tcp>>>(
                m_reactor,
                std::make_shared<io::socket<io::tcp>>(*it)
            );
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(m_log, "unable to connect to node '%s' via endpoint '%s' - [%d] %s", uuid, *it,
                e.code().value(), e.code().message());
            continue;
        }

        break;
    }

    if(!channel) {
        COCAINE_LOG_ERROR(m_log, "unable to connect to node '%s'", hostname);
        return;
    }

    channel->rd->bind(
        std::bind(&locator_t::on_message, this, key, _1),
        std::bind(&locator_t::on_failure, this, key, _1)
    );

    channel->wr->bind(
        std::bind(&locator_t::on_failure, this, key, _1)
    );

    auto timeout = std::make_shared<io::timeout_t>(m_reactor);

    timeout->bind(
        std::bind(&locator_t::on_timeout, this, key)
    );

    // Give the node 60 seconds to respond.
    timeout->start(60.0f);

    auto lifetap = std::make_shared<io::timeout_t>(m_reactor);

    lifetap->bind(
        std::bind(&locator_t::on_lifetap, this, key)
    );

    // Poke the remote node every 5 seconds.
    lifetap->start(0.0f, 5.0f);

    m_remotes[key] = remote_t {
        channel,
        lifetap,
        timeout
    };

    // Start the synchronization.
    channel->wr->write<io::locator::synchronize>(1UL);
}

void
locator_t::on_announce_timer(ev::timer&, int) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    packer << key_type(
        m_context.config.network.uuid,
        m_context.config.network.hostname,
        m_context.config.network.locator
    );

    std::error_code ec;

    if(m_announce->write(buffer.data(), buffer.size(), ec) != static_cast<ssize_t>(buffer.size())) {
        if(ec) {
            COCAINE_LOG_ERROR(m_log, "unable to announce the node - [%d] %s", ec.value(), ec.message());
        } else {
            COCAINE_LOG_ERROR(m_log, "unable to announce the node");
        }
    }
}

namespace {

template<class Container>
struct deferred_erase_action {
    typedef Container container_type;
    typedef typename container_type::key_type key_type;

    void
    operator()() {
        target.erase(key);
    }

    container_type& target;
    const key_type  key;
};

}

void
locator_t::on_message(const key_type& key, const message_t& message) {
    std::string uuid;

    std::tie(uuid, std::ignore, std::ignore) = key;

    switch(message.band()) {
    case 0UL: {
        switch(message.id()) {
        case event_traits<io::rpc::chunk>::id: {
            COCAINE_LOG_DEBUG(m_log, "resetting heartbeat timeout for node '%s' to 60 seconds", uuid);

            m_remotes[uuid].timeout->stop();
            m_remotes[uuid].timeout->start(60.0f);
        } break;

        default:
            COCAINE_LOG_ERROR(m_log, "dropped unknown type %d presence control message", message.id());
        }
    } break;

    case 1UL: {
        switch(message.id()) {
        case event_traits<io::rpc::chunk>::id: {
            std::string chunk;

            message.as<io::rpc::chunk>(chunk);

            msgpack::unpacked unpacked;
            msgpack::unpack(&unpacked, chunk.data(), chunk.size());

            auto dump = unpacked.get().as<synchronize_result_type>();
            auto diff = m_router->update_remote(uuid, dump);

            for(auto it = diff.second.begin(); it != diff.second.end(); ++it) {
                m_gateway->cleanup(uuid, it->first);
            }

            for(auto it = diff.first.begin(); it != diff.first.end(); ++it) {
                m_gateway->consume(uuid, it->first, it->second);
            }
        } break;

        case event_traits<io::rpc::error>::id:
        case event_traits<io::rpc::choke>::id: {
            COCAINE_LOG_INFO(m_log, "node '%s' has been shut down", uuid);

            auto removed = m_router->remove_remote(uuid);

            for(auto it = removed.begin(); it != removed.end(); ++it) {
                m_gateway->cleanup(uuid, it->first);
            }

            // NOTE: It is dangerous to remove the channel while the message is still being
            // processed, so we defer it via reactor_t::post().
            m_reactor.post(deferred_erase_action<decltype(m_remotes)> {
                m_remotes,
                key
            });
        } break;

        default:
            COCAINE_LOG_ERROR(m_log, "dropped unknown type %d synchronization message", message.id());
        }
    }}
}

void
locator_t::on_failure(const key_type& key, const std::error_code& ec) {
    std::string uuid;

    std::tie(uuid, std::ignore, std::ignore) = key;

    if(ec) {
        COCAINE_LOG_WARNING(m_log, "node '%s' has unexpectedly disconnected - [%d] %s", uuid, ec.value(), ec.message());
    } else {
        COCAINE_LOG_WARNING(m_log, "node '%s' has unexpectedly disconnected", uuid);
    }

    auto removed = m_router->remove_remote(uuid);

    for(auto it = removed.begin(); it != removed.end(); ++it) {
        m_gateway->cleanup(uuid, it->first);
    }

    // NOTE: Safe to do since errors are queued up.
    m_remotes.erase(key);
}

void
locator_t::on_timeout(const key_type& key) {
    std::string uuid;

    std::tie(uuid, std::ignore, std::ignore) = key;

    COCAINE_LOG_WARNING(m_log, "node '%s' has timed out", uuid);

    auto removed = m_router->remove_remote(uuid);

    for(auto it = removed.begin(); it != removed.end(); ++it) {
        m_gateway->cleanup(uuid, it->first);
    }

    // NOTE: Safe to do since timeouts are not related to I/O.
    m_remotes.erase(key);
}

void
locator_t::on_lifetap(const key_type& key) {
    std::string uuid;

    std::tie(uuid, std::ignore, std::ignore) = key;

    COCAINE_LOG_DEBUG(m_log, "requesting a heartbeat from node '%s'", uuid);

    m_remotes[uuid].channel->wr->write<io::presence::heartbeat>(0UL);
}
