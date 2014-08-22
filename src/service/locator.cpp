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

#include "cocaine/detail/service/locator.hpp"

#include "cocaine/api/gateway.hpp"
#include "cocaine/api/storage.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/unique_id.hpp"

#include "cocaine/idl/streaming.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/rpc/asio/channel.hpp"
#include "cocaine/rpc/session.hpp"

#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/map.hpp"
#include "cocaine/traits/tuple.hpp"
#include "cocaine/traits/vector.hpp"

#include <blackhole/scoped_attributes.hpp>

#include <boost/asio/connect.hpp>

using namespace blackhole;

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

#include "locator/routing.inl"

// Locator internals

class locator_t::remote_client_t:
    public dispatch<event_traits<locator::connect>::upstream_type>
{
    locator_t* impl;
    const std::string uuid;

public:
    remote_client_t(locator_t* impl_, const std::string& uuid_):
        dispatch<event_traits<locator::connect>::upstream_type>(impl_->name()),
        impl(impl_),
        uuid(uuid_)
    {
        typedef io::protocol<event_traits<locator::connect>::upstream_type>::scope protocol;

        on<protocol::chunk>(std::bind(&remote_client_t::on_announce, this, std::placeholders::_1));
        on<protocol::choke>(std::bind(&remote_client_t::on_shutdown, this));
    }

private:
    void
    on_announce(const connect_result_t& update);

    void
    on_shutdown();
};

void
locator_t::remote_client_t::on_announce(const connect_result_t& update) {
    if(update.empty()) return;

    std::ostringstream stream;
    std::ostream_iterator<std::string> builder(stream, ", ");

    for(auto it = update.begin(); it != update.end(); ++it) {
        *builder++ = it->first;
    }

    COCAINE_LOG_INFO(impl->m_log, "remote node services updated: %s", stream.str())(
        "uuid", uuid
    );

    auto diff = impl->m_router->update_remote(uuid, update);

    for(auto it = diff.second.begin(); it != diff.second.end(); ++it) {
        impl->m_gateway->cleanup(uuid, it->first);
    }

    for(auto it = diff.first.begin(); it != diff.first.end(); ++it) {
        impl->m_gateway->consume(uuid, it->first, it->second);
    }
}

void
locator_t::remote_client_t::on_shutdown() {
    COCAINE_LOG_INFO(impl->m_log, "remote node is shutting down")(
        "uuid", uuid
    );

    impl->m_asio.post(std::bind(&locator_t::drop_node, impl, uuid));
}

// Locator

locator_t::locator_t(context_t& context, io_service& asio, const std::string& name, const dynamic_t& root):
    api::service_t(context, asio, name, root),
    dispatch<locator_tag>(name),
    m_context(context),
    m_log(context.log(name)),
    m_asio(asio),
    m_uuid(unique_id_t().string()),
    m_router(new router_t(*m_log.get()))
{
    using namespace std::placeholders;

    on<locator::resolve>(std::bind(&locator_t::on_resolve, this, _1));
    on<locator::connect>(std::bind(&locator_t::on_connect, this, _1));
    on<locator::refresh>(std::bind(&locator_t::on_refresh, this, _1));
    on<locator::cluster>(std::bind(&locator_t::on_cluster, this));

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
        throw cocaine::error_t("unable to initialize routing groups - %s", e.what());
    }

    if(root.as_object().count("discovery")) {
        const auto& cluster_conf = root.as_object().at("discovery").as_object();
        const auto& cluster_type = cluster_conf.at("type", "unspecified").as_string();
        const auto& cluster_args = cluster_conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' for cluster discovery", cluster_type);

        m_cluster = m_context.get<api::cluster_t>(cluster_type, m_context, *this, name, cluster_args);
    }

    if(root.as_object().count("gateway")) {
        const auto& gateway_conf = root.as_object().at("gateway").as_object();
        const auto& gateway_type = gateway_conf.at("type", "unspecified").as_string();
        const auto& gateway_args = gateway_conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' as a cluster accessor", gateway_type);

        m_gateway = m_context.get<api::gateway_t>(gateway_type, m_context, name, gateway_args);
    }

    // Connect service lifecycle signals.
    context.signals.service.birth.connect(std::bind(&locator_t::on_service, this, _1));
    context.signals.service.death.connect(std::bind(&locator_t::on_service, this, _1));

    // Connect context lifecycle signals.
    context.signals.shutdown.connect(std::bind(&locator_t::on_context_shutdown, this));
}

locator_t::~locator_t() {
    // Empty.
}

auto
locator_t::prototype() const -> const basic_dispatch_t& {
    return *this;
}

io_service&
locator_t::asio() {
    return m_asio;
}

void
locator_t::link_node(const std::string& uuid, const std::vector<tcp::endpoint>& endpoints) {
    if(!m_gateway || m_remotes.find(uuid) != m_remotes.end()) {
        return;
    }

    auto socket = std::make_unique<tcp::socket>(m_asio);

    try {
        auto endpoint = boost::asio::connect(*socket, endpoints.begin());

        COCAINE_LOG_INFO(m_log, "starting synchronization with remote node via %s", *endpoint)(
            "uuid", uuid
        );
    } catch(const boost::system::error_code& e) {
        std::ostringstream stream;
        std::ostream_iterator<tcp::endpoint> builder(stream, ", ");

        std::copy(endpoints.begin(), endpoints.end(), builder);

        COCAINE_LOG_ERROR(m_log, "unable to connect to remote node, tried: %s", stream.str())(
            "uuid", uuid
        );

        return;
    }

    auto channel = std::make_unique<io::channel<tcp>>(std::move(socket));

    using namespace std::placeholders;

    m_remotes[uuid] = std::make_shared<session_t>(std::move(channel), nullptr);
    m_remotes[uuid]->signals.shutdown.connect(std::bind(&locator_t::on_session_shutdown, this, _1, uuid));

    // Start the message dispatching.
    m_remotes[uuid]->pull();

    // Start the synchronization stream.
    m_remotes[uuid]->inject(std::make_shared<remote_client_t>(this, uuid))->send<
        locator::connect
    >(m_uuid);
}

void
locator_t::drop_node(const std::string& uuid) {
    if(!m_gateway || m_remotes.find(uuid) == m_remotes.end()) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "stopping synchronization with remote node")(
        "uuid", uuid
    );

    auto removed = m_router->remove_remote(uuid);

    for(auto it = removed.begin(); it != removed.end(); ++it) {
        m_gateway->cleanup(uuid, it->first);
    }

    m_remotes[uuid]->detach();
    m_remotes.erase(uuid);
}

std::string
locator_t::uuid() const {
    return m_uuid;
}

auto
locator_t::on_resolve(const std::string& name) const -> resolve_result_t {
    auto basename = m_router->select_service(name);
    auto provided = m_context.locate(basename);

    if(provided) {
        COCAINE_LOG_DEBUG(m_log, "providing service using local node")(
            "service", name
        );

        const actor_t& actor = provided.get();

        if(!actor.is_active()) {
            throw cocaine::error_t("service '%s' is not reachable", name);
        }

        const std::vector<tcp::endpoint> endpoints = actor.endpoints();
        const basic_dispatch_t& prototype = actor.prototype();

        return resolve_result_t(endpoints, prototype.versions(), prototype.protocol());
    }

    if(m_gateway) {
        return m_gateway->resolve(basename);
    } else {
        throw cocaine::error_t("service '%s' is not available", name);
    }
}

auto
locator_t::on_connect(const std::string& uuid) -> streamed<connect_result_t> {
    streamed<connect_result_t> stream;

    if(!m_cluster) {
        // No cluster means there are no streams.
        return stream.close();
    }

    scoped_attributes_t attributes(*m_log, {
        attribute::make("uuid", uuid)
    });

    auto ptr = m_locals.synchronize();

    if(ptr->streams.erase(uuid)) {
        COCAINE_LOG_WARNING(m_log, "replacing stale synchronization stream for remote node");
    } else {
        COCAINE_LOG_INFO(m_log, "creating synchronization stream for remote node");
    }

    ptr->streams.insert({uuid, stream});

    return stream.write(ptr->snapshot);
}

auto
locator_t::on_refresh(const std::string& name) -> refresh_result_t {
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

auto
locator_t::on_cluster() const -> cluster_result_t {
    cluster_result_t result;

    for(auto it = m_remotes.begin(); it != m_remotes.end(); ++it) {
        result[it->first] = it->second->remote_endpoint();
    }

    return result;
}

void
locator_t::on_session_shutdown(const boost::system::error_code& ec, const std::string& uuid) {
    if(!ec) return;

    scoped_attributes_t attributes(*m_log, {
        attribute::make("uuid", uuid)
    });

    if(ec != boost::asio::error::eof) {
        COCAINE_LOG_ERROR(m_log, "remote node has disconnected: [%d] %s", ec.value(), ec.message());
    } else {
        COCAINE_LOG_DEBUG(m_log, "remote node has disconnected");
    }

    drop_node(uuid);
}

void
locator_t::on_service(const actor_t& actor) {
    if(!m_cluster) {
        // No cluster means there are no streams.
        return;
    }

    auto ptr = m_locals.synchronize();

    auto metadata = resolve_result_t {
        actor.endpoints(),
        actor.prototype().versions(),
        actor.prototype().protocol()
    };

    if(!ptr->streams.empty()) {
        COCAINE_LOG_DEBUG(m_log, "synchronizing service state with %d remote nodes", ptr->streams.size())(
            "service", actor.prototype().name()
        );

        auto update = connect_result_t {
            { actor.prototype().name(), metadata }
        };

        for(auto it = ptr->streams.begin(); it != ptr->streams.end(); ++it) {
            it->second.write(update);
        }
    }

    if(actor.is_active()) {
        ptr->snapshot[actor.prototype().name()] = metadata;
    } else {
        ptr->snapshot.erase(actor.prototype().name());
    }
}

class locator_t::cleanup_action_t {
    locator_t* impl;

public:
    cleanup_action_t(locator_t* impl_):
        impl(impl_)
    { }

    void
    operator()() {
        if(!impl->m_remotes.empty()) {
            COCAINE_LOG_DEBUG(impl->m_log, "cleaning up %d remote node clients", impl->m_remotes.size());

            for(auto it = impl->m_remotes.begin(); it != impl->m_remotes.end(); ++it) {
                it->second->detach();
            }

            // Disconnect all the remote nodes.
            impl->m_remotes.clear();
        }

        COCAINE_LOG_DEBUG(impl->m_log, "shutting down the clustering infrastructure");

        // Destroy the clustering stuff.
        impl->m_gateway.reset();
        impl->m_cluster.reset();
    }
};

void
locator_t::on_context_shutdown() {
    auto ptr = m_locals.synchronize();

    if(!ptr->streams.empty()) {
        COCAINE_LOG_DEBUG(m_log, "closing %d remote node synchronization streams", ptr->streams.size());

        for(auto it = ptr->streams.begin(); it != ptr->streams.end(); ++it) {
            it->second.close();
        }

        ptr->streams.clear();
    }

    // Finish off the rest of internal state inside the reactor's event loop.
    m_asio.post(cleanup_action_t(this));
}
