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

#include <boost/spirit/include/karma_char.hpp>
#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_string.hpp>

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
    connect_sink_t(locator_t *const parent_, const std::string& uuid_):
        dispatch<event_traits<locator::connect>::upstream_type>(parent_->name() + ":client"),
        parent(parent_),
        uuid(uuid_)
    {
        typedef io::protocol<event_traits<locator::connect>::upstream_type>::scope protocol;

        on<protocol::chunk>(std::bind(&connect_sink_t::on_announce, this, ph::_1, ph::_2));
        on<protocol::choke>(std::bind(&connect_sink_t::on_shutdown, this));
    }

    virtual
   ~connect_sink_t() {
        for(auto it = active.begin(); it != active.end(); ++it) tuple::invoke(
            *it,
            [this, &it](const std::string& name, unsigned int version)
        {
            if(!parent->m_gateway->cleanup(uuid, *it)) parent->m_aggregate[name].erase(version);
        });

        cleanup();
    }

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

void
locator_t::connect_sink_t::discard(const std::error_code& ec) const {
    if(ec.value() == 0) return;

    COCAINE_LOG_ERROR(parent->m_log, "remote client discarded: [%d] %s", ec.value(), ec.message())(
        "uuid", uuid
    );

    parent->drop_node(uuid);
}

void
locator_t::connect_sink_t::cleanup() {
    for(auto it = parent->m_aggregate.begin(), end = parent->m_aggregate.end(); it != end;) {
        if(!it->second.empty()) {
            it++; continue;
        }

        COCAINE_LOG_DEBUG(parent->m_log, "protocol '%s' extinct in the cluster", it->first);

        it = parent->m_aggregate.erase(it);
    }
}

void
locator_t::connect_sink_t::on_announce(const std::string& node,
                                       std::map<std::string, results::resolve>&& update)
{
    if(node != uuid) {
        COCAINE_LOG_ERROR(parent->m_log, "remote client id mismatch: '%s' vs. '%s'", uuid, node);

        parent->drop_node(uuid);
        return;
    }

    if(update.empty()) return;

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

    std::ostringstream stream;
    std::ostream_iterator<char> builder(stream);

    boost::spirit::karma::generate(
        builder,
        boost::spirit::karma::string % ", ",
        update | boost::adaptors::map_keys
    );

    COCAINE_LOG_INFO(parent->m_log, "remote client updated %d service(s): %s", update.size(), stream.str())(
        "uuid", uuid
    );

    cleanup();
}

void
locator_t::connect_sink_t::on_shutdown() {
    COCAINE_LOG_INFO(parent->m_log, "remote client closed the stream")(
        "uuid", uuid
    );

    parent->drop_node(uuid);
}

class locator_t::publish_slot_t: public basic_slot<locator::publish> {
    struct publish_lock_t: public basic_slot<locator::publish>::dispatch_type {
        publish_slot_t *const parent;
        std::string     const handle;

        publish_lock_t(publish_slot_t * const parent_, const std::string& handle_):
            basic_slot<locator::publish>::dispatch_type("publish"),
            parent(parent_),
            handle(handle_)
        {
            on<locator::publish::discard>([this] { discard(std::error_code()); });
        }

        virtual
        void
        discard(const std::error_code& ec) const { parent->discard(ec, handle); }
    };

    typedef std::shared_ptr<const basic_slot::dispatch_type> result_type;

    locator_t *const parent;

public:
    publish_slot_t(locator_t *const parent_): parent(parent_) { }

    auto
    operator()(tuple_type&& args, upstream_type&& upstream) -> boost::optional<result_type> {
        const auto dispatch = cocaine::tuple::invoke(std::move(args),
            [this](std::string&& handle, std::vector<tcp::endpoint>&& location,
                   std::tuple<unsigned int, graph_root_t>&& metadata) -> result_type
        {
            unsigned int versions;
            graph_root_t protocol;

            std::tie(versions, protocol) = metadata;

            if(!protocol.empty() && versions == 0) {
                throw std::system_error(std::make_error_code(std::errc::invalid_argument));
            }

            scoped_attributes_t attributes(*parent->m_log, { attribute::make("service", handle) });

            COCAINE_LOG_INFO(parent->m_log, "publishing external %s service with %d endpoints",
                protocol.empty() ? "non-native" : "native", location.size());
            parent->on_service(handle, results::resolve{location, versions, protocol}, modes::exposed);

            return std::make_shared<publish_lock_t>(this, handle);
        });

        upstream.send<protocol<event_traits<locator::publish>::upstream_type>::scope::value>();

        return boost::make_optional(dispatch);
    }

private:
    void
    discard(const std::error_code& ec, const std::string& handle) {
        scoped_attributes_t attributes(*parent->m_log, { attribute::make("service", handle) });

        COCAINE_LOG_INFO(parent->m_log, "external service disconnected, unpublishing: [%d] %s",
            ec.value(), ec.message());
        return parent->on_service(handle, results::resolve{}, modes::removed);
    }
};

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
    on<locator::routing>(std::bind(&locator_t::on_routing, this, ph::_1, true));

    on<locator::publish>(std::make_shared<publish_slot_t>(this));

    // Service restrictions

    if(!m_cfg.restricted.empty()) {
        std::ostringstream stream;
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::string % ", ", m_cfg.restricted);

        COCAINE_LOG_INFO(m_log, "restricting %d service(s): %s", m_cfg.restricted.size(), stream.str());
    }

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
    auto mapping = m_clients.synchronize();

    if(!m_gateway || mapping->count(uuid) != 0) {
        return;
    }

    auto  socket = std::make_shared<tcp::socket>(m_asio);
    auto& uplink = ((*mapping)[uuid] = {endpoints, api::client<locator_tag>()});

    asio::async_connect(*socket, uplink.endpoints.begin(), uplink.endpoints.end(),
        [=](const std::error_code& ec, std::vector<tcp::endpoint>::const_iterator endpoint)
    {
        auto mapping = m_clients.synchronize();

        scoped_attributes_t attributes(*m_log, { attribute::make("uuid", uuid) });

        if(mapping->count(uuid) == 0) {
            COCAINE_LOG_ERROR(m_log, "remote disappeared while connecting");
            return;
        }

        if(ec) {
            mapping->erase(uuid);

            COCAINE_LOG_ERROR(m_log, "unable to connect to remote: [%d] %s",
                ec.value(), ec.message());
            // TODO: Wrap link_node() in some sort of exponential back-off.
            return m_asio.post([&, uuid, endpoints] { link_node(uuid, endpoints); });
        } else {
            COCAINE_LOG_DEBUG(m_log, "connected to remote via %s", *endpoint);
        }

        auto& client = mapping->at(uuid).client;

        client.attach(m_context.engine().attach(std::make_unique<tcp::socket>(std::move(*socket)),
            nullptr
        ));

        client.invoke<locator::connect>(std::make_shared<connect_sink_t>(this, uuid), m_cfg.uuid);
    });

    COCAINE_LOG_INFO(m_log, "setting up remote client, trying %d route(s)", endpoints.size())(
        "uuid", uuid
    );
}

void
locator_t::drop_node(const std::string& uuid) {
    m_remotes->erase(uuid);

    m_clients.apply([&](client_map_t& mapping) {
        if(!m_gateway || mapping.count(uuid) == 0) {
            return;
        }

        COCAINE_LOG_INFO(m_log, "shutting down remote client")(
            "uuid", uuid
        );

        mapping.erase(uuid);
    });
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

    scoped_attributes_t attributes(*m_log, { attribute::make("service", remapped) });

    if(const auto provided = m_context.locate(remapped)) {
        COCAINE_LOG_DEBUG(m_log, "providing service using local actor");

        return results::resolve {
            provided.get().endpoints(),
            provided.get().prototype().version(),
            provided.get().prototype().root()
        };
    }

    auto lock = m_clients.synchronize();
    auto it   = m_aggregate.end();

    if(m_gateway && (it = m_aggregate.find(remapped)) != m_aggregate.end()) {
        const auto proto = *it->second.begin();

        return results::resolve {
            m_gateway->resolve(api::gateway_t::partition_t{remapped, proto.first}),
            proto.first,
            proto.second
        };
    } else {
        throw std::system_error(error::service_not_available);
    }
}

auto
locator_t::on_connect(const std::string& uuid) -> streamed<results::connect> {
    streamed<results::connect> stream;

    if(!m_cluster) {
        // No cluster means there are no streams.
        return stream.close();
    }

    auto mapping = m_remotes.synchronize();

    scoped_attributes_t attributes(*m_log, { attribute::make("uuid", uuid) });

    if(!mapping->erase(uuid)) {
        COCAINE_LOG_INFO(m_log, "attaching an outgoing stream for locator");
    }

    // Store the stream to synchronize future service updates with the remote node. Updates are
    // sent out on context service signals, and propagate to all nodes in the cluster.
    mapping->insert({uuid, stream});

    // NOTE: Even if there's nothing to return, still send out an empty update.
    return stream.write(m_cfg.uuid, m_snapshots);
}

void
locator_t::on_refresh(const std::vector<std::string>& groups) {
    std::map<std::string, continuum_t::stored_type> values;
    std::map<std::string, continuum_t::stored_type>::iterator lb, ub;

    try {
        const auto storage = api::storage(m_context, "core");
        const auto updated = storage->find("groups", std::vector<std::string>({"group", "active"}));

        for(auto it = groups.begin(); it != groups.end(); ++it) {
            if(std::find(updated.begin(), updated.end(), *it) == updated.end()) {
                continue;
            }

            values.insert({*it, storage->get<continuum_t::stored_type>("groups", *it)});
        }
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to preload routing groups from the storage: [%d] %s",
            e.code().value(), e.code().message());
        throw std::system_error(error::routing_storage_error);
    }

    for(auto it = groups.begin(); it != groups.end(); ++it) m_rgs.apply([&](rg_map_t& mapping) {
        // Routing continuums can't be updated, only erased and reconstructed. This simplifies
        // the logic greatly and doesn't impose any significant performance penalty.
        mapping.erase(*it);

        std::tie(lb, ub) = values.equal_range(*it);

        if(lb == ub) return;

        auto group_log = std::make_unique<logging::log_t>(*m_log, attribute::set_t({
            attribute::make("rg", *it)
        }));

        COCAINE_LOG_INFO(group_log, "routing group %s", lb != ub ? "updated" : "removed");

        mapping.insert({*it, continuum_t(std::move(group_log), lb->second)});
    });

    typedef std::vector<std::string> ruid_vector_t;

    const auto ruids = m_routers.apply([&](const router_map_t& mapping) -> ruid_vector_t {
        return {boost::adaptors::keys(mapping).begin(), boost::adaptors::keys(mapping).end()};
    });

    for(auto it = ruids.begin(); it != ruids.end(); ++it) try {
        on_routing(*it);
    } catch(...) {
        m_routers->erase(*it);
    }

    COCAINE_LOG_DEBUG(m_log, "enqueued sending routing updates to %d router(s)", ruids.size());
}

results::cluster
locator_t::on_cluster() const {
    results::cluster result;

    auto mapping = m_clients.synchronize();

    std::transform(mapping->begin(), mapping->end(), std::inserter(result, result.end()),
        [](const client_map_t::value_type& value) -> results::cluster::value_type
    {
        return {value.first, value.second.client.remote_endpoint()};
    });

    return result;
}

auto
locator_t::on_routing(const std::string& ruid, bool replace) -> streamed<results::routing> {
    auto mapping = m_rgs.synchronize();

    auto results = results::routing();
    auto builder = std::inserter(results, results.end());

    std::transform(mapping->begin(), mapping->end(), builder,
        [](const rg_map_t::value_type& value) -> results::routing::value_type
    {
        return {value.first, value.second.all()};
    });

    auto stream = m_routers.apply([&](router_map_t& mapping) -> streamed<results::routing> {
        if(mapping.count(ruid) == 0 || (replace && mapping.erase(ruid))) {
            COCAINE_LOG_INFO(m_log, "attaching an outgoing stream for router '%s'", ruid);
        }

        return mapping[ruid];
    });

    // NOTE: Even if there's nothing to return, still send out an empty update.
    return stream.write(results);
}

void
locator_t::on_service(const std::string& name, const results::resolve& meta, modes mode) {
    if(m_cfg.restricted.count(name)) {
        return;
    }

    auto mapping = m_remotes.synchronize();

    scoped_attributes_t attributes(*m_log, { attribute::make("service", name) });

    if(mode == modes::exposed) {
        if(m_snapshots.count(name) != 0) {
            COCAINE_LOG_ERROR(m_log, "duplicate service detected");
            return;
        }

        m_snapshots[name] = meta;
    } else {
        m_snapshots.erase(name);
    }

    if(mapping->empty()) return;

    const auto response = results::connect{m_cfg.uuid, {{name, meta}}};

    for(auto it = mapping->begin(); it != mapping->end();) try {
        it->second.write(response);
        it++;
    } catch(...) {
        it = mapping->erase(it);
    }

    COCAINE_LOG_DEBUG(m_log, "enqueued sending service updates to %d locators", mapping->size());
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

        mapping.clear();
    });

    m_cluster = nullptr;

    m_remotes.apply([this](remote_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "closing %d outgoing locator streams", mapping.size());
        }

        boost::for_each(mapping | boost::adaptors::map_values, [](streamed<results::connect>& s) {
            try { s.close(); } catch(...) { /* None */ }
        });
    });

    m_routers.apply([this](router_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "closing %d outgoing routing streams", mapping.size());
        }

        boost::for_each(mapping | boost::adaptors::map_values, [](streamed<results::routing>& s) {
            try { s.close(); } catch(...) { /* None */ }
        });
    });

    m_signals = nullptr;
}

namespace {

// Locator errors

struct locator_category_t:
    public std::error_category
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

} // namespace

namespace cocaine { namespace error {

auto
locator_category() -> const std::error_category& {
    static locator_category_t instance;
    return instance;
}

auto
make_error_code(locator_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), locator_category());
}

}} // namespace cocaine::error
