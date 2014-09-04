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

#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/map.hpp"
#include "cocaine/traits/tuple.hpp"
#include "cocaine/traits/vector.hpp"

#include "cocaine/tuple.hpp"

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
    public dispatch<event_traits<locator::connect>::upstream_type>,
    public std::enable_shared_from_this<remote_client_t>
{
    locator_t * const impl;
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

    void
    on_link(const boost::system::error_code& ec);

    virtual
    void
    discard(const boost::system::error_code& ec) const;

private:
    void
    on_announce(const results::connect& update);

    void
    on_shutdown();
};

void
locator_t::remote_client_t::on_link(const boost::system::error_code& ec) {
    scoped_attributes_t attributes(*impl->m_log, {
        attribute::make("uuid", uuid)
    });

    if(ec) {
        COCAINE_LOG_ERROR(impl->m_log, "unable to connect to remote node: [%d] %s",
            ec.value(), ec.message()
        );

        // Safe to erase directly — client is detached.
        impl->m_remotes.erase(uuid);

        return;
    }

    if(!impl->m_remotes.count(uuid)) {
        COCAINE_LOG_ERROR(impl->m_log, "client has been dropped while connecting to remote node");
        return;
    }

    auto& client = impl->m_remotes.at(uuid);

    COCAINE_LOG_DEBUG(impl->m_log, "connected to remote node via %s", client.session().remote_endpoint());

    client.invoke<locator::connect>(shared_from_this(), impl->m_uuid);
}

void
locator_t::remote_client_t::discard(const boost::system::error_code& ec) const {
    COCAINE_LOG_ERROR(impl->m_log, "remote node has been unexpectedly detached: [%d] %s",
        ec.value(), ec.message()
    )("uuid", uuid);

    impl->drop_node(uuid);
}

void
locator_t::remote_client_t::on_announce(const results::connect& update) {
    if(update.empty()) return;

    std::ostringstream stream;
    std::ostream_iterator<std::string> builder(stream, ", ");

    std::transform(update.begin(), update.end(), builder, tuple::nth_element<0>());

    COCAINE_LOG_INFO(impl->m_log, "remote node has updated the following services: %s", stream.str())(
        "uuid", uuid
    );

    auto diff = impl->m_routing->update_remote(uuid, update);

    for(auto it = diff.second.begin(); it != diff.second.end(); ++it) {
        impl->m_gateway->cleanup(uuid, it->first);
    }

    for(auto it = diff.first.begin(); it != diff.first.end(); ++it) {
        impl->m_gateway->consume(uuid, it->first, it->second);
    }
}

void
locator_t::remote_client_t::on_shutdown() {
    COCAINE_LOG_INFO(impl->m_log, "remote node has closed synchronization stream")(
        "uuid", uuid
    );

    impl->drop_node(uuid);
}

class locator_t::cleanup_action_t {
    locator_t* impl;

public:
    cleanup_action_t(locator_t* impl_):
        impl(impl_)
    { }

    void
    operator()();
};

void
locator_t::cleanup_action_t::operator()() {
    if(!impl->m_remotes.empty()) {
        COCAINE_LOG_DEBUG(impl->m_log, "cleaning up %d remote node client(s)", impl->m_remotes.size());

        // Disconnect all the remote nodes.
        impl->m_remotes.clear();
    }

    // Destroy the loopback locator connection.
    impl->m_resolve = nullptr;

    COCAINE_LOG_DEBUG(impl->m_log, "shutting down clustering components");

    // Destroy the clustering stuff.
    impl->m_gateway = nullptr;
    impl->m_cluster = nullptr;
}

// Locator

locator_t::locator_t(context_t& context, io_service& asio, const std::string& name, const dynamic_t& root):
    api::service_t(context, asio, name, root),
    dispatch<locator_tag>(name),
    m_context(context),
    m_log(context.log(name)),
    m_asio(asio),
    m_uuid(unique_id_t().string()),
    m_resolve(new api::resolve_t(context.log(name + ":resolve"), asio)),
    m_routing(new router_t(*m_log.get()))
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
            m_routing->add_group(*it, storage->get<group_t>("groups", *it));
        }
    } catch(const storage_error_t& e) {
#if defined(HAVE_GCC48)
        std::throw_with_nested(cocaine::error_t("unable to initialize routing groups"));
#else
        throw cocaine::error_t("unable to initialize routing groups");
#endif
    }

    if(root.as_object().count("cluster")) {
        const auto conf = root.as_object().at("cluster").as_object();
        const auto type = conf.at("type", "unspecified").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' for cluster discovery", type);

        m_cluster = m_context.get<api::cluster_t>(type, m_context, *this, name + ":cluster", args);
    }

    if(root.as_object().count("gateway")) {
        const auto conf = root.as_object().at("gateway").as_object();
        const auto type = conf.at("type", "unspecified").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' as a cluster accessor", type);

        m_gateway = m_context.get<api::gateway_t>(type, m_context, name + ":gateway", args);
    }

    // Connect service lifecycle signals.
    context.signals.service.exposed.connect(std::bind(&locator_t::on_service, this, _1));
    context.signals.service.removed.connect(std::bind(&locator_t::on_service, this, _1));

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

    COCAINE_LOG_INFO(m_log, "starting synchronization with remote node")(
        "uuid", uuid
    );

    m_resolve->connect(m_remotes[uuid], endpoints, std::bind(&remote_client_t::on_link,
        std::make_shared<remote_client_t>(this, uuid),
        std::placeholders::_1
    ));
}

void
locator_t::drop_node(const std::string& uuid) {
    if(!m_gateway || m_remotes.find(uuid) == m_remotes.end()) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "stopping synchronization with remote node")(
        "uuid", uuid
    );

    auto removed = m_routing->remove_remote(uuid);

    for(auto it = removed.begin(); it != removed.end(); ++it) {
        m_gateway->cleanup(uuid, it->first);
    }

    m_remotes.erase(uuid);
}

std::string
locator_t::uuid() const {
    return m_uuid;
}

auto
locator_t::on_resolve(const std::string& name) const -> results::resolve {
    auto basename = m_routing->select_service(name);
    auto provided = m_context.locate(basename);

    if(provided && provided.get().is_active()) {
        COCAINE_LOG_DEBUG(m_log, "providing service using local node")(
            "service", name
        );

        return results::resolve {
            provided.get().endpoints(),
            provided.get().prototype().version(),
            provided.get().prototype().graph()
        };
    }

    if(m_gateway) {
        return m_gateway->resolve(basename);
    } else {
        throw boost::system::system_error(error::service_not_available);
    }
}

auto
locator_t::on_connect(const std::string& uuid) -> streamed<results::connect> {
    streamed<results::connect> stream;

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

void
locator_t::on_refresh(const std::string& name) {
    std::vector<std::string> groups;

    // It's here to keep the reference alive.
    const auto storage = api::storage(m_context, "core");

    try {
        groups = storage->find("groups", std::vector<std::string>({
            "group",
            "active"
        }));
    } catch(const storage_error_t& e) {
        throw boost::system::system_error(error::routing_storage_error);
    }

    if(std::find(groups.begin(), groups.end(), name) == groups.end()) {
        return m_routing->remove_group(name);
    }

    typedef std::map<std::string, unsigned int> group_t;

    try {
        m_routing->add_group(name, storage->get<group_t>("groups", name));
    } catch(const storage_error_t& e) {
        throw boost::system::system_error(error::routing_storage_error);
    }
}

auto
locator_t::on_cluster() const -> results::cluster {
    results::cluster result;

    for(auto it = m_remotes.begin(); it != m_remotes.end(); ++it) {
        result[it->first] = it->second.session().remote_endpoint();
    }

    return result;
}

void
locator_t::on_service(const actor_t& actor) {
    if(!m_cluster) {
        // No cluster means there are no streams.
        return;
    }

    auto ptr = m_locals.synchronize();

    auto metadata = results::resolve {
        actor.endpoints(),
        actor.prototype().version(),
        actor.prototype().graph()
    };

    if(!ptr->streams.empty()) {
        COCAINE_LOG_DEBUG(m_log, "synchronizing service state with %d remote node(s)", ptr->streams.size())(
            "service", actor.prototype().name()
        );

        auto update = results::connect {
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

void
locator_t::on_context_shutdown() {
    auto ptr = m_locals.synchronize();

    if(!ptr->streams.empty()) {
        COCAINE_LOG_DEBUG(m_log, "closing %d remote node synchronization stream(s)", ptr->streams.size());

        for(auto it = ptr->streams.begin(); it != ptr->streams.end(); ++it) {
            it->second.close();
        }

        ptr->streams.clear();
    }

    // Schedule the rest of internal state cleanup inside the reactor's event loop.
    m_asio.post(cleanup_action_t(this));
}

namespace {

// Locator errors

struct locator_category_t:
    public boost::system::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.service.locator";
    }

    virtual
    auto
    message(int code) const -> std::string {
        switch(code) {
          case cocaine::error::locator_errors::service_not_available:
            return "service is not available";
          case cocaine::error::locator_errors::routing_storage_error:
            return "routing storage is unavailable";
        }

        return "cocaine.service.locator error";
    }
};

auto
locator_category() -> const boost::system::error_category& {
    static locator_category_t instance;
    return instance;
}

} // namespace

namespace cocaine { namespace error {

auto
make_error_code(locator_errors code) -> boost::system::error_code {
    return boost::system::error_code(static_cast<int>(code), locator_category());
}

}} // namespace cocaine::error
