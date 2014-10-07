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

#include <boost/range/adaptor/map.hpp>

#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_stream.hpp>
#include <boost/spirit/include/karma_string.hpp>

using namespace blackhole;

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace cocaine::io;
using namespace cocaine::service;

#include "locator/routing.inl"

// Locator internals

class locator_t::remote_client_t:
    public dispatch<event_traits<locator::connect>::upstream_type>,
    public std::enable_shared_from_this<remote_client_t>
{
    locator_t *const parent;
    const std::string uuid;

public:
    remote_client_t(locator_t* parent_, const std::string& uuid_):
        dispatch<event_traits<locator::connect>::upstream_type>(parent_->name()),
        parent(parent_),
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
    scoped_attributes_t attributes(*parent->m_log, {
        attribute::make("uuid", uuid)
    });

    if(ec) {
        COCAINE_LOG_ERROR(parent->m_log, "unable to connect to remote node: [%d] %s",
            ec.value(), ec.message()
        );

        // Safe to erase directly — client is detached.
        parent->m_remotes.erase(uuid);

        return;
    }

    if(!parent->m_remotes.count(uuid)) {
        COCAINE_LOG_ERROR(parent->m_log, "client has been dropped while connecting to remote node");
        return;
    }

    auto& client  = parent->m_remotes.at(uuid);
    auto& session = client.session().get();

    COCAINE_LOG_DEBUG(parent->m_log, "connected to remote node via %s", session.remote_endpoint());

    client.invoke<locator::connect>(shared_from_this(), parent->m_cfg.uuid);
}

void
locator_t::remote_client_t::discard(const boost::system::error_code& ec) const {
    COCAINE_LOG_ERROR(parent->m_log, "remote node has been unexpectedly detached: [%d] %s",
        ec.value(), ec.message()
    )("uuid", uuid);

    parent->drop_node(uuid);
}

void
locator_t::remote_client_t::on_announce(const results::connect& update) {
    if(update.empty()) return;

    std::ostringstream stream;
    std::ostream_iterator<char> builder(stream);

    boost::spirit::karma::generate(
        builder,
        boost::spirit::karma::stream % ", ",
        update | boost::adaptors::map_keys
    );

    COCAINE_LOG_INFO(parent->m_log, "remote node has updated the following services: %s", stream.str())(
        "uuid", uuid
    );

    const auto diff = parent->m_routing->update_remote(uuid, update);

    for(auto it = diff.second.begin(); it != diff.second.end(); ++it) {
        parent->m_gateway->cleanup(uuid, it->first);
    }

    for(auto it = diff.first.begin(); it != diff.first.end(); ++it) {
        parent->m_gateway->consume(uuid, it->first, it->second);
    }
}

void
locator_t::remote_client_t::on_shutdown() {
    COCAINE_LOG_INFO(parent->m_log, "remote node has closed synchronization stream")(
        "uuid", uuid
    );

    parent->drop_node(uuid);
}

class locator_t::cleanup_action_t {
    locator_t *const parent;

public:
    cleanup_action_t(locator_t* parent_):
        parent(parent_)
    { }

    void
    operator()();
};

void
locator_t::cleanup_action_t::operator()() {
    if(!parent->m_remotes.empty()) {
        COCAINE_LOG_DEBUG(parent->m_log, "cleaning up %d remote node client(s)", parent->m_remotes.size());

        // Disconnect all the remote nodes.
        parent->m_remotes.clear();
    }

    COCAINE_LOG_DEBUG(parent->m_log, "shutting down clustering components");

    // Destroy the clustering stuff.
    parent->m_gateway = nullptr;
    parent->m_cluster = nullptr;

    // Destroy the loopback locator connection.
    parent->m_resolve = nullptr;
}

// Locator

locator_cfg_t::locator_cfg_t(const std::string& name, const dynamic_t& root):
    uuid(unique_id_t().string())
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
    m_resolve(new api::resolve_t(context.log(name + ":resolve"), asio, {})),
    m_routing(new router_t(*m_log.get()))
{
    using namespace std::placeholders;

    on<locator::resolve>(std::bind(&locator_t::on_resolve, this, _1));
    on<locator::connect>(std::bind(&locator_t::on_connect, this, _1));
    on<locator::refresh>(std::bind(&locator_t::on_refresh, this, _1));
    on<locator::cluster>(std::bind(&locator_t::on_cluster, this));

    // Service restrictions

    if(!m_cfg.restricted.empty()) {
        std::ostringstream stream;
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::stream % ", ", m_cfg.restricted);

        COCAINE_LOG_INFO(m_log, "restricting %d service(s): %s", m_cfg.restricted.size(), stream.str());
    }

    // Context shutdown signal is set to track 'm_routing' because its lifetime essentially matches
    // that of the Locator service itself

    context.signals.shutdown.connect(context_t::signals_t::context_signals_t::slot_type(
        std::bind(&locator_t::on_context_shutdown, this)
    ).track_foreign(m_routing));

    // Initialize clustering components

    if(root.as_object().count("cluster")) {
        const auto conf = root.as_object().at("cluster").as_object();
        const auto type = conf.at("type", "unspecified").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' for cluster discovery", type);

        m_cluster = m_context.get<api::cluster_t>(type, m_context, *this, name + ":cluster", args);

        // Connect service signals. Signals are set to track 'm_cluster' because it is responsible
        // of handling them, so no cluster object - no service lifecycle signal handling

        context.signals.service.exposed.connect(context_t::signals_t::service_signals_t::slot_type(
            std::bind(&locator_t::on_service, this, _1)
        ).track_foreign(m_cluster));

        context.signals.service.removed.connect(context_t::signals_t::service_signals_t::slot_type(
            std::bind(&locator_t::on_service, this, _1)
        ).track_foreign(m_cluster));
    }

    if(root.as_object().count("gateway")) {
        const auto conf = root.as_object().at("gateway").as_object();
        const auto type = conf.at("type", "unspecified").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' as a cluster accessor", type);

        m_gateway = m_context.get<api::gateway_t>(type, m_context, name + ":gateway", args);
    }

    // It's here to keep the reference alive.
    const auto storage = api::storage(m_context, "core");

    try {
        const auto groups = storage->find("groups", std::vector<std::string>({
            "group",
            "active"
        }));

        typedef std::map<std::string, unsigned int> group_t;

        for(auto it = groups.begin(); it != groups.end(); ++it) {
            m_routing->update_group(*it, storage->get<group_t>("groups", *it));
        }
    } catch(const storage_error_t& e) {
#if defined(HAVE_GCC48)
        std::throw_with_nested(cocaine::error_t("unable to initialize routing groups"));
#else
        throw cocaine::error_t("unable to initialize routing groups");
#endif
    }
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

    const auto removed = m_routing->remove_remote(uuid);

    for(auto it = removed.begin(); it != removed.end(); ++it) {
        m_gateway->cleanup(uuid, it->first);
    }

    m_remotes.erase(uuid);
}

std::string
locator_t::uuid() const {
    return m_cfg.uuid;
}

auto
locator_t::on_resolve(const std::string& name) const -> results::resolve {
    const auto basename = m_routing->select_service(name);
    const auto provided = m_context.locate(basename);

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

    std::lock_guard<std::mutex> guard(m_mutex);

    if(m_streams.erase(uuid)) {
        COCAINE_LOG_WARNING(m_log, "replacing stale synchronization stream for remote node");
    } else {
        COCAINE_LOG_INFO(m_log, "creating synchronization stream for remote node");
    }

    m_streams.insert({uuid, stream});

    return stream.write(m_snapshot);
}

void
locator_t::on_refresh(const std::vector<std::string>& groups) {
    typedef std::map<std::string, unsigned int> group_t;

    std::map<std::string, group_t> values;
    std::map<std::string, group_t>::iterator lb, ub;

    try {
        const auto storage = api::storage(m_context, "core");
        const auto updated = storage->find("groups", std::vector<std::string>({"group", "active"}));

        for(auto it = groups.begin(); it != groups.end(); ++it) {
            if(std::find(updated.begin(), updated.end(), *it) == updated.end()) {
                continue;
            }

            values.insert({*it, storage->get<group_t>("groups", *it)});
        }
    } catch(const storage_error_t& e) {
        throw boost::system::system_error(error::routing_storage_error);
    }

    for(auto it = groups.begin(); it != groups.end(); ++it) {
        std::tie(lb, ub) = values.equal_range(*it);

        if(lb == ub) {
            m_routing->remove_group(*it);
        } else {
            m_routing->update_group(*it, lb->second);
        }
    }

    COCAINE_LOG_INFO(m_log, "updated %d active group(s)", values.size());
}

auto
locator_t::on_cluster() const -> results::cluster {
    results::cluster result;

    for(auto it = m_remotes.begin(); it != m_remotes.end(); ++it) {
        result[it->first] = it->second.session().get().remote_endpoint();
    }

    return result;
}

void
locator_t::on_service(const actor_t& actor) {
    if(m_cfg.restricted.count(actor.prototype().name())) {
        return;
    }

    const auto metadata = results::resolve {
        actor.endpoints(),
        actor.prototype().version(),
        actor.prototype().graph()
    };

    const auto response = results::connect {
        { actor.prototype().name(), metadata }
    };

    std::lock_guard<std::mutex> guard(m_mutex);

    COCAINE_LOG_DEBUG(m_log, "synchronizing service state with %d remote node(s)", m_streams.size())(
        "service", actor.prototype().name()
    );

    for(auto it = m_streams.begin(); it != m_streams.end();) {
        try {
            it->second.write(response); ++it;
        } catch(...) {
            COCAINE_LOG_INFO(m_log, "removing synchronization stream for remote node")(
                "uuid", it->first
            );

            // The remote node has crashed, was stopped or something else happened which renders it
            // impossible to continue delivering synchronization diffs to it.
            it = m_streams.erase(it);
        }
    }

    if(actor.is_active()) {
        m_snapshot[actor.prototype().name()] = metadata;
    } else {
        m_snapshot.erase(actor.prototype().name());
    }
}

void
locator_t::on_context_shutdown() {
    std::lock_guard<std::mutex> guard(m_mutex);

    COCAINE_LOG_DEBUG(m_log, "closing %d remote node synchronization stream(s)", m_streams.size());

    for(auto it = m_streams.begin(); it != m_streams.end();) {
        try {
            it->second.close();
        } catch(...) {
            // Ignore all exceptions. The runtime is being destroyed anyway.
        }

        it = m_streams.erase(it);
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
