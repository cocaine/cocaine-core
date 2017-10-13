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

#include <algorithm>
#include <cmath>

#include "cocaine/api/gateway.hpp"
#include "cocaine/api/storage.hpp"

#include "cocaine/context.hpp"
#include "cocaine/context/signal.hpp"
#include "cocaine/context/quote.hpp"
#include "cocaine/engine.hpp"

#include "cocaine/idl/primitive.hpp"
#include "cocaine/idl/streaming.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/repository/cluster.hpp"
#include "cocaine/repository/gateway.hpp"
#include "cocaine/repository/storage.hpp"

#include "cocaine/rpc/actor.hpp"
#include "cocaine/rpc/asio/encoder.hpp"

#include "cocaine/traits/dynamic.hpp"
#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/map.hpp"
#include "cocaine/traits/vector.hpp"

#include "cocaine/unique_id.hpp"

#include <asio/connect.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>
#include <blackhole/wrapper.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/numeric.hpp>

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

using namespace asio;
using namespace asio::ip;

namespace ph = std::placeholders;

using blackhole::attribute_list;
using blackhole::scope::holder_t;

// Locator internals

class locator_t::connect_sink_t: public dispatch<event_traits<locator::connect>::upstream_type> {
    locator_t  *const parent;
    std::string const uuid;

public:
    connect_sink_t(locator_t *const parent_, const std::string& uuid_):
        dispatch<event_traits<locator::connect>::upstream_type>(parent_->name() + ":client"),
        parent(parent_),
        uuid(uuid_)
    {
        typedef io::protocol<event_traits<locator::connect>::upstream_type>::scope protocol;

        on<protocol::chunk>().execute(std::bind(&connect_sink_t::on_announce, this, ph::_1, ph::_2, ph::_3));
        on<protocol::choke>(std::bind(&connect_sink_t::on_shutdown, this));
    }

    virtual
   ~connect_sink_t() {
        parent->m_gateway->cleanup(uuid);
    }

    virtual
    void
    discard(const std::error_code& ec);

private:
    void
    on_announce(hpack::headers_t headers, const std::string& node, std::map<std::string, results::resolve>&& update);

    void
    on_shutdown();
};

void
locator_t::connect_sink_t::discard(const std::error_code& ec) {
    if(ec.value() == 0) return;

    COCAINE_LOG_ERROR(parent->m_log, "remote client discarded: [{:d}] {}", ec.value(), ec.message(), attribute_list({
        {"uuid", uuid}
    }));

    parent->drop_node(uuid);
}

void
locator_t::connect_sink_t::on_announce(hpack::headers_t headers, const std::string& node,
                                       std::map<std::string, results::resolve>&& update)
{
    if(node != uuid) {
        COCAINE_LOG_ERROR(parent->m_log, "remote client id mismatch: '{}' vs. '{}'", uuid, node);

        parent->drop_node(uuid);
        return;
    }

    if(update.empty()) return;

    auto lock = parent->m_clients.synchronize();

    for(auto it = update.begin(); it != update.end(); ++it) tuple::invoke(
        std::move(it->second),
        [&](std::vector<tcp::endpoint>&& location, unsigned int versions, graph_root_t&& protocol)
    {
        std::string name(it->first);

        if(location.empty()) {
            parent->m_gateway->cleanup(uuid, name);
        } else {
            dynamic_t::object_t extra;
            auto extra_data = hpack::header::find_first(headers, "x-cocaine-locator-extra");
            if(extra_data) {
                msgpack::unpacked unpacked;
                size_t offset = 0;
                msgpack::unpack(&unpacked, extra_data->value().data(), extra_data->value().size(), &offset);
                io::type_traits<std::map<std::string, dynamic_t>>::unpack(unpacked.get(), extra);
            }
            parent->m_gateway->consume(uuid, name, versions, location, protocol, extra);
        }
    });

    const auto joined = boost::algorithm::join(update | boost::adaptors::map_keys, ", ");

    COCAINE_LOG_INFO(parent->m_log, "remote client updated {:d} service(s): {}", update.size(), joined, attribute_list({
        {"uuid", uuid}
    }));
}

void
locator_t::connect_sink_t::on_shutdown() {
    COCAINE_LOG_INFO(parent->m_log, "remote client closed its stream", {
        {"uuid", uuid}
    });

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
            on<locator::publish::discard>([this] { discard({}); });
        }

        virtual
        void
        discard(const std::error_code& ec) { parent->discard(ec, handle); }
    };

    typedef std::shared_ptr<basic_slot::dispatch_type> result_type;

    locator_t *const parent;

public:
    publish_slot_t(locator_t *const parent_): parent(parent_) { }

    auto
    operator()(tuple_type&& args,
               upstream_type&& upstream) -> boost::optional<result_type>
    {
        return operator()({}, std::move(args), std::move(upstream));
    }

    auto
    operator()(const std::vector<hpack::header_t>&,
               tuple_type&& args,
               upstream_type&& upstream) -> boost::optional<result_type>
    {
        const auto dispatch = cocaine::tuple::invoke(std::move(args),
            [this](std::string&& handle, std::vector<tcp::endpoint>&& location,
                   std::tuple<unsigned int, graph_root_t>&& metadata) -> result_type
        {
            const holder_t scoped(*parent->m_log, {{"service", handle}});

            unsigned int versions;
            graph_root_t protocol;

            std::tie(versions, protocol) = metadata;

            if(!protocol.empty() && versions == 0) {
                throw std::system_error(error::missing_version_error);
            }

            COCAINE_LOG_INFO(parent->m_log, "publishing {} external service with {:d} endpoints",
                protocol.empty() ? "non-native" : "native",
                location.size());

            parent->on_service(handle, results::resolve{location, versions, protocol}, modes::exposed);

            return std::make_shared<publish_lock_t>(this, handle);
        });

        upstream.send<protocol<event_traits<locator::publish>::upstream_type>::scope::value>();

        return boost::make_optional(dispatch);
    }

private:
    void
    discard(const std::error_code& ec, const std::string& handle) {
        const holder_t scoped(*parent->m_log, {{"service", handle}});

        COCAINE_LOG_INFO(parent->m_log, "external service disconnected, unpublishing: [{:d}] {}",
            ec.value(), ec.message());

        return parent->on_service(handle, results::resolve{}, modes::removed);
    }
};

class locator_t::routing_slot_t: public basic_slot<locator::routing> {
    struct routing_lock_t: public basic_slot<locator::routing>::dispatch_type {
        routing_slot_t *const parent;
        std::string     const handle;

        routing_lock_t(routing_slot_t *const parent_, const std::string& handle_):
            basic_slot<locator::routing>::dispatch_type("routing"),
            parent(parent_),
            handle(handle_)
        {
            on<locator::routing::discard>([this] { discard({}); });
        }

        virtual
        void
        discard(const std::error_code& ec) { parent->discard(ec, handle); }
    };

    typedef std::shared_ptr<basic_slot::dispatch_type> result_type;

    locator_t *const parent;

public:
    routing_slot_t(locator_t *const parent_): parent(parent_) { }

    auto
    operator()(tuple_type&& args,
               upstream_type&& upstream) -> boost::optional<result_type>
    {
        return operator()({}, std::move(args), std::move(upstream));
    }

    auto
    operator()(const std::vector<hpack::header_t>&,
               tuple_type&& args,
               upstream_type&& upstream) -> boost::optional<result_type>
    {
        const auto ruid = std::get<0>(args);

        auto rv = parent->on_routing(ruid, true);
        auto dispatch = std::make_shared<routing_lock_t>(this, ruid);

        // Try to flush the initial routing group information (if available). This can throw.
        rv.attach(std::move(upstream));

        return boost::make_optional(result_type(dispatch));
    }

private:
    void
    discard(const std::error_code& ec, const std::string& handle) {
        COCAINE_LOG_DEBUG(parent->m_log, "detaching outgoing stream for router '{}': [{:d}] {}",
            handle,
            ec.value(), ec.message());

        parent->m_routers->erase(handle);
    }
};

// Locator

locator_cfg_t::locator_cfg_t(const std::string& name_, const dynamic_t& root):
    name(name_),
    extra_param(root.as_object().at("extra_param", dynamic_t::empty_object).as_object())
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
    m_asio(asio),
    link_attempts(0),
    link_timer()
{
    on<locator::resolve>(std::bind(&locator_t::on_resolve, this, ph::_1, ph::_2));
    on<locator::connect>(std::bind(&locator_t::on_connect, this, ph::_1));
    on<locator::refresh>(std::bind(&locator_t::on_refresh, this, ph::_1));
    on<locator::cluster>(std::bind(&locator_t::on_cluster, this));

    on<locator::publish>(std::make_shared<publish_slot_t>(this));
    on<locator::routing>(std::make_shared<routing_slot_t>(this));

    on<locator::uuid>([&](){
        return uuid();
    });

    // Service restrictions

    if(!m_cfg.restricted.empty()) {
        const auto restricted_str = boost::algorithm::join(m_cfg.restricted, ", ");

        COCAINE_LOG_INFO(m_log, "restricting {:d} service(s): {}", m_cfg.restricted.size(), restricted_str);
    }

    // Context signals slot

    m_signals = std::make_shared<dispatch<context_tag>>(name);
    m_signals->on<io::context::shutdown>(std::bind(&locator_t::on_context_shutdown, this));

    // Clustering components

    m_signals->on<io::context::service::exposed>(std::bind(&locator_t::on_service, this,
                                                           ph::_1, ph::_2, modes::exposed));
    m_signals->on<io::context::service::removed>(std::bind(&locator_t::on_service, this,
                                                           ph::_1, ph::_2, modes::removed));

    if(root.as_object().count("gateway")) {
        const auto conf = root.as_object().at("gateway").as_object();
        const auto type = conf.at("type", "adhoc").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '{}' as a gateway manager, enabling service routing", type);

        m_gateway = m_context.repository().get<api::gateway_t>(type, m_context, uuid(), name + ":gateway", args,
                                                               m_cfg.extra_param);
    }

    if(root.as_object().count("cluster")) {
        const auto conf = root.as_object().at("cluster").as_object();
        const auto type = conf.at("type", "unspecified").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '{}' as a cluster manager, enabling synchronization", type);

        api::cluster_t::mode_t mode = m_gateway ? api::cluster_t::mode_t::full : api::cluster_t::mode_t::announce_only;
        m_cluster = m_context.repository().get<api::cluster_t>(type, m_context, *this, mode, name + ":cluster", args);
    }


    // It's here to keep the reference alive.
    const auto storage = api::storage(m_context, "core");

    try {
        const auto groups = storage->find("groups", std::vector<std::string>({"group", "active"})).get();
        on_refresh(groups);
    } catch(const std::system_error& e) {
        throw std::system_error(e.code(), "unable to initialize routing groups");
    }

    context.signal_hub().listen(m_signals, asio);
}

locator_t::~locator_t() {
    // Empty.
}

basic_dispatch_t&
locator_t::prototype() {
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
    auto& uplink = ((*mapping)[uuid] = {endpoints, nullptr});

    asio::async_connect(*socket, uplink.endpoints.begin(), uplink.endpoints.end(),
        [=](const std::error_code& ec, std::vector<tcp::endpoint>::const_iterator endpoint)
    {
        const holder_t scoped(*m_log, {{"uuid", uuid}});

        auto session = m_clients.apply(
            [&](client_map_t& mapping) -> std::shared_ptr<cocaine::session<tcp>>
        {
            if(mapping.count(uuid) == 0) {
                COCAINE_LOG_ERROR(m_log, "remote disappeared while connecting");
                return nullptr;
            }

            if(ec) {
                COCAINE_LOG_ERROR(m_log, "unable to connect to remote: [{:d}] {}", ec.value(), ec.message());
                mapping.erase(uuid);

                retry_link_node(uuid, endpoints);
                return nullptr;
            }

            link_timer.apply([&](std::unique_ptr<asio::deadline_timer>& timer) {
                timer.reset();
                link_attempts = 0;
            });
            COCAINE_LOG_INFO(m_log, "connected to remote via {}", *endpoint);

            // Uniquify the socket object.
            auto ptr = std::make_unique<tcp::socket>(std::move(*socket));

            std::shared_ptr<cocaine::session<tcp>> session;
            try {
                session = m_context.engine().attach(std::move(ptr), nullptr);
                mapping.at(uuid).ptr = session;
            } catch (const std::system_error& err) {
                COCAINE_LOG_ERROR(m_log, "unable to set up remote client: {}", error::to_string(err));
                mapping.erase(uuid);

                retry_link_node(uuid, endpoints);
            }

            return session;
        });

        // Something went wrong in the session creation code above, bail out.
        if(!session) return;

        auto upstream = session->fork(std::make_shared<connect_sink_t>(this, uuid));

        try {
            upstream->send<locator::connect>(this->uuid());
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to set up remote stream: {}", error::to_string(e));
            m_clients->erase(uuid);
        }
    });

    COCAINE_LOG_INFO(m_log, "setting up remote client, trying {:d} route(s)", endpoints.size(), attribute_list({
        {"uuid", uuid}
    }));
}

void
locator_t::drop_node(const std::string& uuid) {
    std::shared_ptr<session<tcp>> session;

    m_clients.apply([&](client_map_t& mapping) {
        auto it = mapping.find(uuid);

        if(!m_gateway || it == mapping.end()) {
            return;
        }

        COCAINE_LOG_INFO(m_log, "shutting down remote client", attribute_list({
            {"uuid", uuid}
        }));

        session = it->second.ptr;
        mapping.erase(it);
    });

    if(session) {
        session->detach(std::error_code());
    }
}

std::string
locator_t::uuid() const {
    return m_context.uuid();
}

auto
locator_t::retry_link_node(const std::string& uuid, const std::vector<asio::ip::tcp::endpoint>& endpoints) -> void {
    link_timer.apply([&](std::unique_ptr<asio::deadline_timer>& timer) {
        if (timer) {
            // Do nothing if the timer is already locked and loaded.
            return;
        }

        timer.reset(new asio::deadline_timer(m_asio));
        timer->expires_from_now(boost::posix_time::seconds(std::min(static_cast<int>(std::pow(2, link_attempts)), 32)));
        timer->async_wait([=](const std::error_code& ec) {
            switch (ec.value()) {
            case asio::error::operation_aborted:
                return;
            default:
                ;
            }

            link_node(uuid, endpoints);
        });

        link_attempts += 1;
    });
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

    const holder_t scoped(*m_log, {{"service", remapped}});

    // If we don't have gateway or it resolves only remote services try to lookup service locally
    // first.
    if(!m_gateway || m_gateway->resolve_policy() == api::gateway_t::resolve_policy_t::remote_only) {
        const auto provided = m_context.locate(remapped);
        if(provided) {
            COCAINE_LOG_DEBUG(m_log, "providing service using local actor");
            return results::resolve {
                provided->endpoints,
                provided->prototype->version(),
                provided->prototype->root()
            };
        }
    }

    // Service is not found in the context and we don't have gateway to lookup at.
    if(!m_gateway) {
        throw std::system_error(error::service_not_available);
    }

    auto provided = m_gateway->resolve(remapped);
    return results::resolve {
        std::move(provided.endpoints),
        std::move(provided.version),
        std::move(provided.protocol)
    };
}

auto
locator_t::on_connect(const std::string& uuid) -> streamed<results::connect> {
    streamed<results::connect> stream;

    const holder_t scoped(*m_log, {{"uuid", uuid}});

    auto mapping = m_remotes.synchronize();

    if(!m_cluster) {
        // No cluster means there are no streams.
        stream.close();
        return stream;
    }

    if(mapping->erase(uuid) == 0) {
        COCAINE_LOG_INFO(m_log, "attaching outgoing stream for locator");
    }

    // Store the stream to synchronize future service updates with the remote node. Updates are
    // sent out on context service signals, and propagate to all nodes in the cluster.
    mapping->insert({uuid, stream});

    stream.write(prepare_extra(), this->uuid(), m_snapshots);
    return stream;
}

void
locator_t::on_refresh(const std::vector<std::string>& groups) {
    typedef std::vector<std::string> ruid_vector_t;

    const auto storage = api::storage(m_context, "core");
    const auto updated = storage->find("groups", std::vector<std::string>({"group", "active"})).get();

    m_rgs.apply([&](rg_map_t& original) {
        // Make a deep copy of the original routing group mapping to use as the accumulator, for
        // guaranteed atomicity of routing group updates.
        rg_map_t clone = original;

        original = std::move(std::accumulate(groups.begin(), groups.end(), std::ref(clone),
            [&](rg_map_t& result, const std::string& group) -> std::reference_wrapper<rg_map_t>
        {
            const holder_t scoped(*m_log, {{"rg", group}});

            result.erase(group);

            if(std::find(updated.begin(), updated.end(), group) == updated.end()) {
                COCAINE_LOG_INFO(m_log, "removing routing group");

                // There's no routing group with this name in the storage anymore, so do nothing.
                return std::ref(result);
            }

            try {
                COCAINE_LOG_INFO(m_log, "updating routing group");

                result.insert(std::make_pair(group, continuum_t(
                    std::make_unique<blackhole::wrapper_t>(*m_log, blackhole::attributes_t()),
                    storage->get<continuum_t::stored_type>("groups", group).get())));
            } catch(const std::system_error& e) {
                COCAINE_LOG_ERROR(m_log, "unable to pre-load routing group data for update: {}",
                    error::to_string(e));
                throw std::system_error(error::routing_storage_error);
            }

            return std::ref(result);
        }).get());
    });

    auto ruids = m_routers.apply([](const router_map_t& routers) {
        ruid_vector_t ruids;
        ruids.reserve(routers.size());
        for(const auto& router: routers) {
            ruids.push_back(router.first);
        }
        return ruids;
    });

    for(auto it = ruids.begin(); it != ruids.end(); ++it) try {
        on_routing(*it);
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(m_log, "unable to enqueue routing updates for router '{}': {}",
            *it,
            error::to_string(e));
        m_routers->erase(*it);
    }

    COCAINE_LOG_DEBUG(m_log, "enqueued sending routing updates to {:d} router(s)", ruids.size());
}

results::cluster
locator_t::on_cluster() const {
    return m_clients.apply([&](const client_map_t& clients) mutable {
        results::cluster result;
        for(const auto& client: clients) {
            const auto& session = client.second.ptr;

            // NOTE: Some sessions might be nullptr because there is a connection attempt in progress.
            result[client.first] = session ? session->remote_endpoint() : ip::tcp::endpoint();
        }
        return result;
    });
}

auto
locator_t::on_routing(const std::string& ruid, bool replace) -> streamed<results::routing> {
    auto results = results::routing();
    auto builder = std::inserter(results, results.end());

    boost::transform(*m_rgs.synchronize(), builder,
        [](const rg_map_t::value_type& value) -> results::routing::value_type
    {
        return {value.first, value.second.all()};
    });

    auto stream = m_routers.apply([&](router_map_t& mapping) -> streamed<results::routing> {
        if(mapping.count(ruid) == 0 || (replace && mapping.erase(ruid))) {
            COCAINE_LOG_INFO(m_log, "attaching outgoing stream for router '{}'", ruid);
        }

        return mapping[ruid];
    });

    // NOTE: Even if there's nothing to return, still send out an empty update.
    if(auto ec = stream.write(results)) {
        throw std::system_error(ec, format("failed to write to outgoing stream '{}'", ruid));
    }
    return stream;
}

void
locator_t::on_service(const std::string& name, const results::resolve& meta, modes mode) {
    if(m_gateway) {
        if(mode == modes::exposed) {
            m_gateway->consume(uuid(), name, std::get<1>(meta), std::get<0>(meta), std::get<2>(meta), m_cfg.extra_param);
        } else {
            m_gateway->cleanup(uuid(), name);
        }
    }
    if(m_cfg.restricted.count(name) || !m_cluster) {
        return;
    }

    const holder_t scoped(*m_log, {{"service", name}});

    auto mapping = m_remotes.synchronize();

    if(mode == modes::exposed) {
        if(m_snapshots.count(name) != 0) {
            COCAINE_LOG_ERROR(m_log, "duplicate service detected");
            return;
        }

        m_snapshots[name] = meta;
    } else {
        m_snapshots.erase(name);
    }

    const auto response = results::connect{uuid(), {{name, meta}}};

    for(auto it = mapping->begin(); it != mapping->end(); /***/) try {
        it->second.write(prepare_extra(), response);
        it++;
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(m_log, "unable to enqueue service updates for locator '{}': {}",
            it->first,
            error::to_string(e));
        it = mapping->erase(it);
    }

    COCAINE_LOG_DEBUG(m_log, "enqueued sending service updates to {} locators", mapping->size());
}

void
locator_t::on_context_shutdown() {
    COCAINE_LOG_DEBUG(m_log, "shutting down distributed components");

    m_clients.apply([this](client_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "shutting down {:d} remote client(s)", mapping.size());
        }

        mapping.clear();
    });

    m_remotes.apply([this](remote_map_t& mapping) {
        m_cluster = nullptr;

        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "closing {:d} outgoing locator streams", mapping.size());
        }

        boost::for_each(mapping | boost::adaptors::map_values, [](streamed<results::connect>& s) {
            s.close();
        });
    });

    m_routers.apply([this](router_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "closing {:d} outgoing routing streams", mapping.size());
        }

        boost::for_each(mapping | boost::adaptors::map_values, [](streamed<results::routing>& s) {
            s.close();
        });
    });

    m_signals = nullptr;
}

auto
locator_t::prepare_extra() -> hpack::headers_t {
    io::aux::encoded_buffers_t buffer;
    msgpack::packer<io::aux::encoded_buffers_t> packer(buffer);
    io::type_traits<std::map<std::string, dynamic_t>>::pack(packer, m_cfg.extra_param);
    hpack::headers_t extra;
    extra.push_back(hpack::header_t("x-cocaine-locator-extra", std::string(buffer.data(), buffer.size())));
    return extra;
}
