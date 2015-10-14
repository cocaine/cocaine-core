/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2013-2015 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/engine.hpp"

#include "cocaine/idl/primitive.hpp"
#include "cocaine/idl/streaming.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/rpc/actor.hpp"

#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/map.hpp"
#include "cocaine/traits/vector.hpp"

#include "cocaine/unique_id.hpp"

#include <asio/connect.hpp>

#include <blackhole/scoped_attributes.hpp>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/transform.hpp>

#include <boost/range/numeric.hpp>

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

using namespace asio;
using namespace asio::ip;

using namespace blackhole;

namespace ph = std::placeholders;

// Locator internals

class locator_t::connect_sink_t: public dispatch<event_traits<locator::connect>::upstream_type> {
    locator_t  *const parent;
    std::string const uuid;

    // Currently announced services.
    std::set<api::gateway_t::partition_t> active;

public:
    connect_sink_t(locator_t *const parent_, const std::string& uuid_);

    virtual
   ~connect_sink_t();

    virtual
    void
    discard(const std::error_code& ec) const;

private:
    void
    cleanup();

    void
    on_announce(const std::string& node, std::map<std::string, results::resolve>&& update);

    void
    on_shutdown();
};

locator_t::connect_sink_t::connect_sink_t(locator_t *const parent_, const std::string& uuid_):
    dispatch(parent_->name() + "/client"),
    parent(parent_),
    uuid(uuid_)
{
    typedef io::protocol<event_traits<locator::connect>::upstream_type>::scope protocol;

    on<protocol::chunk>(std::bind(&connect_sink_t::on_announce, this, ph::_1, ph::_2));
    on<protocol::choke>(std::bind(&connect_sink_t::on_shutdown, this));
}

locator_t::connect_sink_t::~connect_sink_t() {
    auto lock = parent->m_clients.synchronize();

    for(auto it = active.begin(); it != active.end(); ++it) tuple::invoke(
        *it,
        [&](const std::string& name, unsigned int versions)
    {
        if(!parent->m_gateway->cleanup(uuid, *it)) parent->m_aggregate[name].erase(versions);
    });

    cleanup();
}

void
locator_t::connect_sink_t::discard(const std::error_code& ec) const {
    if(ec.value() == 0) {
        return;
    }

    COCAINE_LOG_ERROR(parent->m_log, "remote client discarded: [%d] %s", ec.value(), ec.message())(
        "uuid", uuid
    );

    parent->drop_node(uuid);
}

void
locator_t::connect_sink_t::cleanup() {
    for(auto it = parent->m_aggregate.begin(), end = parent->m_aggregate.end(); it != end; /***/) {
        if(!it->second.empty()) {
            it++; continue;
        }

        COCAINE_LOG_DEBUG(parent->m_log, "protocol '%s' extinct in the cluster", it->first);

        it = parent->m_aggregate.erase(it);
    }
}

void
locator_t::connect_sink_t::on_announce(const std::string& node, std::map<std::string, results::resolve>&& update) {
    if(node != uuid) {
        COCAINE_LOG_ERROR(parent->m_log, "remote client id mismatch: '%s' vs. '%s'", uuid, node);
        return parent->drop_node(uuid);
    } else if(update.empty()) {
        COCAINE_LOG_DEBUG(parent->m_log, "remote client sent an empty announce");
        return;
    }

    auto lock = parent->m_clients.synchronize();

    for(auto it = update.begin(); it != update.end(); ++it) tuple::invoke(
        std::move(it->second),
        [&](std::vector<tcp::endpoint>&& location, unsigned int versions, graph_root_t&& protocol)
    {
        int copies = 0;
        api::gateway_t::partition_t partition(it->first, versions);

        if(location.empty()) {
            copies = parent->m_gateway->cleanup(uuid, partition);
            active.erase (partition);
        } else {
            copies = parent->m_gateway->consume(uuid, partition, location);
            active.insert(partition);
        }

        if(copies == 0) {
            parent->m_aggregate[it->first].erase(versions);
        } else {
            parent->m_aggregate[it->first][versions] = std::move(protocol);
        }
    });

    const auto joined = boost::algorithm::join(update | boost::adaptors::map_keys, ", ");

    COCAINE_LOG_INFO(parent->m_log, "received %d service announce(s): %s", update.size(), joined)(
        "uuid", uuid
    );

    cleanup();
}

void
locator_t::connect_sink_t::on_shutdown() {
    COCAINE_LOG_INFO(parent->m_log, "remote client closed its stream")(
        "uuid", uuid
    );

    parent->drop_node(uuid);
}

class locator_t::publish_slot_t: public basic_slot<locator::publish> {
    locator_t *const parent;

    class publish_sink_t: public basic_slot::dispatch_type {
        publish_slot_t *const parent;
        std::string     const handle;

    public:
        publish_sink_t(publish_slot_t *const parent_, const std::string& handle_):
            basic_slot::dispatch_type("publish/discard"),
            parent(parent_),
            handle(handle_)
        {
            on<locator::publish::discard>([this] { discard(std::error_code{}); });
        }

        virtual
        void
        discard(const std::error_code& ec) const { parent->on_discard(ec, handle); }
    };

    // Slot's transition type.
    typedef boost::optional<std::shared_ptr<const basic_slot::dispatch_type>> result_type;

public:
    publish_slot_t(locator_t *const parent_): parent(parent_) { }

    auto
    operator()(tuple_type&& args, upstream_type&& upstream) -> result_type;

private:
    void
    on_discard(const std::error_code& ec, const std::string& handle);
};

auto
locator_t::publish_slot_t::operator()(tuple_type&& args, upstream_type&& upstream) -> result_type {
    const auto dispatch = cocaine::tuple::invoke(std::move(args),
        [this](std::string&& handle, std::vector<tcp::endpoint>&& location,
               std::tuple<unsigned int, graph_root_t>&& metadata) -> result_type::value_type
    {
        scoped_attributes_t attributes(*parent->m_log, { attribute::make("service", handle) });

        unsigned int versions;
        graph_root_t protocol;

        std::tie(versions, protocol) = metadata;

        if(!protocol.empty() && versions == 0) {
            throw std::system_error(error::missing_version_error);
        }

        COCAINE_LOG_INFO(parent->m_log, "publishing %s external service with %d endpoints",
            protocol.empty() ? "non-native" : "native",
            location.size());

        parent->on_service(handle, results::resolve{location, versions, protocol}, modes::exposed);

        return std::make_shared<publish_sink_t>(this, handle);
    });

    // TODO(@kobolog): Way too verbose, replace with deferred<void> in-place close() call.
    upstream.send<protocol<event_traits<locator::publish>::upstream_type>::scope::value>();

    return result_type(dispatch);
}

void
locator_t::publish_slot_t::on_discard(const std::error_code& ec, const std::string& handle) {
    scoped_attributes_t attributes(*parent->m_log, { attribute::make("service", handle) });

    COCAINE_LOG_INFO(parent->m_log, "external service disconnected, unpublishing: [%d] %s",
        ec.value(), ec.message());

    return parent->on_service(handle, results::resolve{}, modes::removed);
}

class locator_t::routing_slot_t: public basic_slot<locator::routing> {
    locator_t *const parent;

    class routing_sink_t: public basic_slot::dispatch_type {
        routing_slot_t *const parent;
        std::string     const handle;

    public:
        routing_sink_t(routing_slot_t *const parent_, const std::string& handle_):
            basic_slot::dispatch_type("routing/discard"),
            parent(parent_),
            handle(handle_)
        {
            on<locator::routing::discard>([this] { discard(std::error_code{}); });
        }

        virtual
        void
        discard(const std::error_code& ec) const { parent->on_discard(ec, handle); }
    };

    // Slot's transition type.
    typedef boost::optional<std::shared_ptr<const basic_slot::dispatch_type>> result_type;

public:
    routing_slot_t(locator_t *const parent_): parent(parent_) { }

    auto
    operator()(tuple_type&& args, upstream_type&& upstream) -> result_type;

private:
    void
    on_discard(const std::error_code& ec, const std::string& handle);
};

auto
locator_t::routing_slot_t::operator()(tuple_type&& args, upstream_type&& upstream) -> result_type {
    const auto dispatch = tuple::invoke(std::move(args),
        [&](std::string&& ruid) -> result_type::value_type
    {
        auto rv = parent->on_routing(ruid, true);

        // Try to flush the initial routing group information (if available).
        rv.attach(std::move(upstream));

        return std::make_shared<routing_sink_t>(this, ruid);
    });

    return result_type(dispatch);
}

void
locator_t::routing_slot_t::on_discard(const std::error_code& ec, const std::string& handle) {
    COCAINE_LOG_DEBUG(parent->m_log, "detaching outgoing stream for router '%s': [%d] %s",
        handle,
        ec.value(), ec.message());

    parent->m_routers->erase(handle);
}

// Locator

locator_cfg_t::locator_cfg_t(const std::string& name_, const dynamic_t& root):
    name(name_),
    uuid(root.as_object().at("uuid", unique_id_t().string()).as_string())
{
    restricted = root.as_object().at("restrict", dynamic_t::array_t()).to<std::set<std::string>>();
    restricted.insert(name);
}

locator_t::locator_t(context_t& context, io_service& asio, const std::string& name, const dynamic_t& root):
    category_type(context, asio, name, root),
    dispatch<locator_tag>(name),
    m_context(context),
    m_log(context.log(name)),
    m_cfg(name, root),
    m_asio(asio)
{
    on<locator::resolve>(std::bind(&locator_t::on_resolve, this, ph::_1, ph::_2));
    on<locator::connect>(std::bind(&locator_t::on_connect, this, ph::_1));
    on<locator::refresh>(std::bind(&locator_t::on_refresh, this, ph::_1));
    on<locator::cluster>(std::bind(&locator_t::on_cluster, this));

    on<locator::publish>(std::make_shared<publish_slot_t>(this));
    on<locator::routing>(std::make_shared<routing_slot_t>(this));

    // Context signals slot

    m_signals = std::make_shared<dispatch<context_tag>>(name);
    m_signals->on<context::shutdown>(std::bind(&locator_t::on_context_shutdown, this));

    // Clustering components

    if(root.as_object().count("cluster")) {
        const auto conf = root.as_object().at("cluster").as_object();
        const auto type = conf.at("type", "unspecified").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' as a cluster manager, enabling synchronization", type);

        m_signals->on<context::service::exposed>(std::bind(&locator_t::on_service, this,
            ph::_1, ph::_2, modes::exposed));
        m_signals->on<context::service::removed>(std::bind(&locator_t::on_service, this,
            ph::_1, ph::_2, modes::removed));

        m_cluster = m_context.get<api::cluster_t>(type, m_context, *this, name + ":cluster", args);
    }

    if(root.as_object().count("gateway")) {
        const auto conf = root.as_object().at("gateway").as_object();
        const auto type = conf.at("type", "unspecified").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' as a gateway manager, enabling service routing", type);

        m_gateway = m_context.get<api::gateway_t>(type, m_context, name + ":gateway", args);
    }

    // It's here to keep the reference alive.
    const auto storage = api::storage(m_context, "core");

    try {
        const auto groups = storage->find("groups", std::vector<std::string>({"group", "active"}));
        on_refresh(groups);
    } catch(const std::system_error& e) {
        throw std::system_error(e.code(), "unable to initialize routing groups");
    }

    context.listen(m_signals, asio);
}

locator_t::~locator_t() {
    // Empty.
}

const basic_dispatch_t&
locator_t::prototype() const {
    return *this;
}

io_service&
locator_t::asio() {
    return m_asio;
}

void
locator_t::link_node(const std::string& uuid, const std::vector<tcp::endpoint>& endpoints) {
    if(m_clients.apply([&](client_map_t& mapping) {
        return !m_gateway || mapping[uuid] != nullptr;
    })) {
        return;
    };

    typedef std::pair<std::vector<tcp::endpoint>, std::unique_ptr<tcp::socket>> uplink_type;

    const auto uplink = std::make_shared<uplink_type>(
        // This vector should be kept alive because of the retarded ASIO API.
        endpoints,
        std::make_unique<tcp::socket>(m_asio)
    );

    asio::async_connect(*uplink->second, uplink->first.begin(), uplink->first.end(),
        [=](const std::error_code& ec, std::vector<tcp::endpoint>::const_iterator endpoint)
    {
        scoped_attributes_t attributes(*m_log, { attribute::make("uuid", uuid) });

        auto session = m_clients.apply(
            [&](client_map_t& mapping) -> client_map_t::mapped_type
        {
            auto ptr = std::move(uplink->second);

            if(mapping.count(uuid) == 0) {
                COCAINE_LOG_ERROR(m_log, "remote has been lost during the connection attempt");
            } else if(ec) {
                COCAINE_LOG_ERROR(m_log, "unable to connect: [%d] %s", ec.value(), ec.message());
                mapping.erase(uuid);
            } else {
                return (mapping.at(uuid) = m_context.engine().attach(std::move(ptr), nullptr));
            }

            return nullptr;
        });

        if(session != nullptr) try {
            COCAINE_LOG_DEBUG(m_log, "establishing incoming locator stream via %s", *endpoint);
            session->fork(std::make_shared<connect_sink_t>(this, uuid))->send<locator::connect>(m_cfg.uuid);
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to setup remote client: %s", error::to_string(e));
            m_clients->erase(uuid);
        }
    });

    COCAINE_LOG_INFO(m_log, "setting up remote client, trying %d endpoint(s)", endpoints.size())(
        "uuid", uuid
    );
}

void
locator_t::drop_node(const std::string& uuid) {
    m_remotes->erase(uuid);

    auto session = m_clients.apply(
        [&](client_map_t& mapping) -> client_map_t::mapped_type
    {
        auto it = mapping.find(uuid);

        if(!m_gateway || it == mapping.end()) {
            return nullptr;
        }

        COCAINE_LOG_INFO(m_log, "shutting down remote client")(
            "uuid", uuid
        );

        auto session = std::move(it->second); mapping.erase(it);

        return session;
    });

    if(session != nullptr) {
        session->detach(std::error_code{});
    }
}

std::string
locator_t::uuid() const {
    return m_cfg.uuid;
}

results::resolve
locator_t::on_resolve(const std::string& name, const std::string& seed) const {
    const auto remapped = m_rgs.apply([&](const rg_map_t& mapping) -> std::string {
        if(!mapping.count(name)) {
            return name;
        } else {
            return seed.empty() ? mapping.at(name).get() : mapping.at(name).get(seed);
        }
    });

    // TODO(@kobolog):
    //   - Separate aggregate and client synchronization, like in RGs vs. Routers.

    scoped_attributes_t attributes(*m_log, { attribute::make("service", remapped) });

    const auto remote = m_clients.apply(
        [&](const client_map_t& /***/) -> boost::optional<quote_t>
    {
        auto it = m_aggregate.end();

        if(!m_gateway || (it = m_aggregate.find(remapped)) == m_aggregate.end()) {
            return boost::none;
        }

        auto protocol = graph_root_t{};
        auto version  = 0u;

        // Aggregate is (version, protocol) tuples sorted by version in descending order, so
        // the most recent protocol version is chosen here and resolved against the Gateway.
        std::tie(version, protocol) = *it->second.begin();

        auto location = m_gateway->resolve(api::gateway_t::partition_t{remapped, version});

        return quote_t{std::move(location), version, std::move(protocol)};
    });

    if(const auto quoted = m_context.locate(remapped)) {
        return results::resolve{quoted->location, quoted->version, quoted->protocol};
    } else if(remote) {
        return results::resolve{remote->location, remote->version, remote->protocol};
    } else {
        throw std::system_error(error::service_not_available);
    }
}

auto
locator_t::on_connect(const std::string& uuid) -> streamed<results::connect> {
    scoped_attributes_t attributes(*m_log, { attribute::make("uuid", uuid) });

    return m_remotes.apply([&](remote_map_t& mapping) -> streamed<results::connect> {
        remote_map_t::iterator lb, ub;

        std::tie(lb, ub) = mapping.equal_range(uuid);

        if(lb != ub) {
            lb->second.close(); mapping.erase(lb);
        } else {
            COCAINE_LOG_INFO(m_log, "attaching outgoing stream for remote locator");
        }

        // Create an unattached stream. Stream will be attached on function return.
        streamed<results::connect> stream;

        // Store the stream to synchronize future service updates with the remote node. Updates are
        // sent out on context service signals, and propagate to all nodes in the cluster.
        mapping.insert(ub, std::make_pair(uuid, stream));

        // NOTE: Even if there's nothing to return, still send out an empty update.
        return stream.write(m_cfg.uuid, m_snapshots);
    });
}

void
locator_t::on_refresh(const std::vector<std::string>& groups) {
    typedef std::vector<std::string> ruid_vector_t;

    const auto storage = api::storage(m_context, "core");
    const auto updated = storage->find("groups", std::vector<std::string>({"group", "active"}));

    m_rgs.apply([&](rg_map_t& original) {
        // Make a deep copy of the original routing group mapping to use as the accumulator, for
        // guaranteed atomicity of routing group updates.
        rg_map_t clone = original;

        original = std::move(std::accumulate(groups.begin(), groups.end(), std::ref(clone),
            [&](rg_map_t& result, const std::string& group) -> std::reference_wrapper<rg_map_t>
        {
            scoped_attributes_t attributes(*m_log, { attribute::make("rg", group) });

            result.erase(group);

            if(std::find(updated.begin(), updated.end(), group) == updated.end()) {
                COCAINE_LOG_INFO(m_log, "removing routing group");

                // There's no routing group with this name in the storage anymore, so do nothing.
                return std::ref(result);
            }

            try {
                COCAINE_LOG_INFO(m_log, "updating routing group");

                result.insert(std::make_pair(group, continuum_t(
                    std::make_unique<logging::log_t>(*m_log, attribute::set_t{}),
                    storage->get<continuum_t::stored_type>("groups", group))));
            } catch(const std::system_error& e) {
                COCAINE_LOG_ERROR(m_log, "unable to preload routing group data for update: %s",
                    error::to_string(e));
                throw std::system_error(error::routing_storage_error);
            }

            return std::ref(result);
        }).get());
    });

    const auto ruids = boost::accumulate(*m_routers.synchronize(), ruid_vector_t{},
        [](ruid_vector_t result, const router_map_t::value_type& value) -> ruid_vector_t
    {
        result.push_back(value.first); return result;
    });

    for(auto it = ruids.begin(); it != ruids.end(); ++it) try {
        on_routing(*it);
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(m_log, "unable to enqueue routing update for router '%s': %s",
            *it,
            error::to_string(e));

        m_routers->erase(*it);
    }

    COCAINE_LOG_DEBUG(m_log, "enqueued routing updates for %d router(s)", ruids.size());
}

results::cluster
locator_t::on_cluster() const {
    return boost::accumulate(*m_clients.synchronize(), results::cluster{},
        [](results::cluster result, const client_map_t::value_type& value) -> results::cluster
    {
        const auto& session = value.second;

        // NOTE: Some sessions might be nullptr because there is a connection attempt in progress.
        result[value.first] = session ? session->remote_endpoint() : ip::tcp::endpoint{};

        return result;
    });
}

auto
locator_t::on_routing(const std::string& ruid, bool replace) -> streamed<results::routing> {
    auto results = results::routing{};
    auto builder = std::inserter(results, results.end());

    boost::transform(*m_rgs.synchronize(), builder,
        [](const rg_map_t::value_type& value) -> results::routing::value_type
    {
        return {value.first, value.second.all()};
    });

    auto stream = m_routers.apply([&](router_map_t& mapping) -> streamed<results::routing> {
        router_map_t::iterator lb, ub;

        std::tie(lb, ub) = mapping.equal_range(ruid);

        if(replace && lb != ub) {
            lb->second.close(); mapping.erase(lb);
        } else {
            COCAINE_LOG_INFO(m_log, "attaching outgoing stream for router '%s'", ruid);
        }

        return mapping[ruid];
    });

    // NOTE: Even if there's nothing to return, still send out an empty update.
    return stream.write(results);
}

void
locator_t::on_service(const std::string& name, const results::resolve& meta, modes mode) {
    scoped_attributes_t attributes(*m_log, { attribute::make("service", name) });

    if(m_cfg.restricted.count(name)) {
        COCAINE_LOG_DEBUG(m_log, "ignoring service update due to restrictions");
        return;
    }

    // TODO(@kobolog):
    //   - Separate snapshotting and remote stream synchronization, like in RGs vs. Routers.
    //   - Send updates out from outside the critical section.
    //   - Batch updates into reasonable-sized announces and flush every few seconds.

    m_remotes.apply([&](remote_map_t& mapping) {
        const auto update = results::connect{m_cfg.uuid, {{name, meta}}};

        if(mode == modes::exposed) {
            if(m_snapshots.count(name) != 0) {
                COCAINE_LOG_ERROR(m_log, "duplicate service detected");
                return;
            }

            m_snapshots[name] = meta;
        } else {
            m_snapshots.erase(name);
        }

        for(auto it = mapping.begin(); it != mapping.end(); /***/) try {
            it->second.write(update);
            it++;
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(m_log, "unable to enqueue service update for locator '%s': %s",
                it->first,
                error::to_string(e));

            it = mapping.erase(it);
        }

        COCAINE_LOG_DEBUG(m_log, "enqueued service updates for %d locator(s)", mapping.size());
    });
}

void
locator_t::on_context_shutdown() {
    COCAINE_LOG_DEBUG(m_log, "shutting down distributed components");

    m_clients.apply([this](client_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "shutting down %d remote client(s)", mapping.size());
        }

        boost::for_each(mapping | boost::adaptors::map_values, [](client_map_t::mapped_type& ptr) {
            ptr->detach(std::error_code{});
            ptr = nullptr;
        });
    });

    m_remotes.apply([this](remote_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "closing %d outgoing locator stream(s)", mapping.size());
        }

        boost::for_each(mapping | boost::adaptors::map_values, [](streamed<results::connect>& s) {
            try { s.close(); } catch(...) { /***/ }
        });
    });

    m_routers.apply([this](router_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "closing %d outgoing routing stream(s)", mapping.size());
        }

        boost::for_each(mapping | boost::adaptors::map_values, [](streamed<results::routing>& s) {
            try { s.close(); } catch(...) { /***/ }
        });
    });

    m_signals->halt();
}
