/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#include "cocaine/detail/locator.hpp"

#include "cocaine/api/gateway.hpp"
#include "cocaine/api/storage.hpp"

#include "cocaine/asio/reactor.hpp"
#include "cocaine/asio/resolver.hpp"
#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/tcp.hpp"
#include "cocaine/asio/udp.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"

#include "cocaine/idl/streaming.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/rpc/channel.hpp"
#include "cocaine/rpc/session.hpp"

#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/tuple.hpp"

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/bind/bind.hpp>

using namespace cocaine;
using namespace cocaine::io;

using namespace std::placeholders;

#include "routing.inl"

locator_t::locator_t(context_t& context, reactor_t& reactor):
    dispatch<io::locator_tag>("service/locator"),
    m_context(context),
    m_log(new logging::log_t(context, "service/locator")),
    m_reactor(reactor),
    m_router(new router_t(*m_log.get()))
{
    // NOTE: Slot for the io::locator::synchronize action is bound in context_t::bootstrap(), as
    // it's easier to implement it using context_t internals.
    on<io::locator::resolve>(std::bind(&locator_t::resolve, this, _1));
    on<io::locator::refresh>(std::bind(&locator_t::refresh, this, _1));

    COCAINE_LOG_INFO(m_log, "this node's id is '%s'", m_context.config.network.uuid);

    // It's here to keep the reference alive.
    const auto storage = api::storage(m_context, "core");

    try {
        auto groups = storage->find("groups", std::vector<std::string>({
            "group",
            "active"
        }));

        typedef std::map<std::string, unsigned int> group_t;

        for(auto it = groups.begin(); it != groups.end(); ++it) {
            m_router->add_group(*it, storage->get<group_t>("groups", *it));
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

    // It's here to keep the reference alive.
    const auto storage = api::storage(m_context, "core");

    try {
        groups = storage->find("groups", std::vector<std::string>({
            "group",
            "active"
        }));
    } catch(const storage_error_t& e) {
        throw cocaine::error_t("unable to read the routing group list - %s", e.what());
    }

    if(std::find(groups.begin(), groups.end(), name) != groups.end()) {
        typedef std::map<std::string, unsigned int> group_t;

        try {
            m_router->add_group(name, storage->get<group_t>("groups", name));
        } catch(const storage_error_t& e) {
            throw cocaine::error_t("unable to read routing group '%s' - %s", name, e.what());
        }
    } else {
        m_router->remove_group(name);
    }
}

namespace {

template<class Container>
struct deferred_erase_action {
    typedef Container container_type;
    typedef typename container_type::key_type key_type;

    void
    operator()() {
        container.erase(key);
    }

    container_type& container;
    const key_type  key;
};

} // namespace

class locator_t::remote_client_t:
    public dispatch<io::event_traits<io::locator::synchronize>::drain_type>
{
    locator_t& impl;

    // Remote node identification.
    const remote_id_t node;
    const std::string uuid;

public:
    typedef io::protocol<
        io::event_traits<io::locator::synchronize>::drain_type
    >::scope protocol;

    struct announce_slot_t:
        public basic_slot<protocol::chunk>
    {
        announce_slot_t(const std::shared_ptr<remote_client_t>& impl_):
            impl(impl_)
        { }

        typedef basic_slot<protocol::chunk>::dispatch_type dispatch_type;
        typedef basic_slot<protocol::chunk>::tuple_type tuple_type;
        typedef basic_slot<protocol::chunk>::upstream_type upstream_type;

        std::shared_ptr<dispatch_type>
        operator()(tuple_type&& args, upstream_type&& /* upstream */) {
            auto service = impl.lock();

            tuple::invoke(
                boost::bind(&remote_client_t::announce, service.get(), boost::arg<1>()),
                args
            );

            return service;
        }

    private:
        const std::weak_ptr<remote_client_t> impl;
    };

    struct shutdown_slot_t:
        public basic_slot<protocol::choke>
    {
        shutdown_slot_t(const std::shared_ptr<remote_client_t>& impl_):
            impl(impl_)
        { }

        typedef basic_slot<protocol::choke>::dispatch_type dispatch_type;
        typedef basic_slot<protocol::choke>::tuple_type tuple_type;
        typedef basic_slot<protocol::choke>::upstream_type upstream_type;

        std::shared_ptr<dispatch_type>
        operator()(tuple_type&& /* args */, upstream_type&& /* upstream */) {
            impl.lock()->shutdown();

            // Return an empty protocol dispatch.
            return std::shared_ptr<dispatch_type>();
        }

    private:
        const std::weak_ptr<remote_client_t> impl;
    };

    remote_client_t(locator_t& impl_, const remote_id_t& node_):
        dispatch<io::event_traits<io::locator::synchronize>::drain_type>(impl_.name()),
        impl(impl_),
        node(node_),
        uuid(std::get<0>(node))
    { }

private:
    void
    announce(const result_of<io::locator::synchronize>::type& dump) {
        COCAINE_LOG_INFO(impl.m_log, "node '%s' has been updated", uuid);

        auto diff = impl.m_router->update_remote(uuid, dump);

        for(auto it = diff.second.begin(); it != diff.second.end(); ++it) {
            impl.m_gateway->cleanup(uuid, it->first);
        }

        for(auto it = diff.first.begin(); it != diff.first.end(); ++it) {
            impl.m_gateway->consume(uuid, it->first, it->second);
        }
    }

    void
    shutdown() {
        COCAINE_LOG_INFO(impl.m_log, "node '%s' has been shut down", uuid);

        auto removed = impl.m_router->remove_remote(uuid);

        for(auto it = removed.begin(); it != removed.end(); ++it) {
            impl.m_gateway->cleanup(uuid, it->first);
        }

        // NOTE: It is dangerous to disconnect the remote while the message is still being
        // processed, so we defer it via reactor_t::post().
        impl.m_reactor.post(deferred_erase_action<decltype(impl.m_remotes)>{impl.m_remotes, node});
    }
};

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

    try {
        msgpack::unpack(&unpacked, buffers, size);
    } catch(const msgpack::unpack_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to decode an announce");
        return;
    }

    remote_id_t node;

    try {
        unpacked.get() >> node;
    } catch(const msgpack::type_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to decode an announce");
        return;
    }

    if(m_remotes.find(node) != m_remotes.end()) {
        return;
    }

    std::string uuid;
    std::string hostname;
    uint16_t    port;

    std::tie(uuid, hostname, port) = node;

    COCAINE_LOG_INFO(m_log, "discovered node '%s' on '%s:%d'", uuid, hostname, port);

    std::vector<io::tcp::endpoint> endpoints;

    try {
        endpoints = io::resolver<io::tcp>::query(hostname, port);
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to resolve node '%s' endpoints - [%d] %s", uuid, e.code().value(),
            e.code().message());
        return;
    }

    std::unique_ptr<io::channel<io::socket<io::tcp>>> channel;

    for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        try {
            channel = std::make_unique<io::channel<io::socket<io::tcp>>>(
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
        std::bind(&locator_t::on_message, this, node, _1),
        std::bind(&locator_t::on_failure, this, node, _1)
    );

    channel->wr->bind(
        std::bind(&locator_t::on_failure, this, node, _1)
    );

    // Start the synchronization

    auto service = std::make_shared<remote_client_t>(*this, node);

    typedef remote_client_t::protocol protocol;

    service->on<protocol::chunk>(std::make_shared<remote_client_t::announce_slot_t>(service));
    service->on<protocol::choke>(std::make_shared<remote_client_t::shutdown_slot_t>(service));

    // Spawn the synchronization session

    m_remotes[node] = std::make_shared<session_t>(std::move(channel));
    m_remotes[node]->invoke(service)->send<io::locator::synchronize>();
}

void
locator_t::on_announce_timer(ev::timer&, int) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    packer << remote_id_t(
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

void
locator_t::on_message(const remote_id_t& node, const message_t& message) {
    auto it = m_remotes.find(node);

    if(it == m_remotes.end()) {
        return;
    }

    it->second->invoke(message);
}

void
locator_t::on_failure(const remote_id_t& node, const std::error_code& ec) {
    std::string uuid;

    std::tie(uuid, std::ignore, std::ignore) = node;

    if(m_remotes.find(node) == m_remotes.end()) {
        return;
    } else if(ec) {
        COCAINE_LOG_WARNING(m_log, "node '%s' has unexpectedly disconnected - [%d] %s", uuid, ec.value(), ec.message());
    } else {
        COCAINE_LOG_WARNING(m_log, "node '%s' has unexpectedly disconnected", uuid);
    }

    auto removed = m_router->remove_remote(uuid);

    for(auto it = removed.begin(); it != removed.end(); ++it) {
        m_gateway->cleanup(uuid, it->first);
    }

    m_remotes[node]->detach();
    m_remotes.erase(node);
}
