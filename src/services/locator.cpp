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

#include "cocaine/detail/services/locator.hpp"

#include "cocaine/api/gateway.hpp"
#include "cocaine/api/storage.hpp"

#include "cocaine/asio/reactor.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/unique_id.hpp"

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
using namespace cocaine::service;

using namespace std::placeholders;

#include "locator/routing.inl"

locator_t::locator_t(context_t& context, reactor_t& reactor, const std::string& name, const dynamic_t& root):
    api::service_t(context, reactor, name, root),
    dispatch<io::locator_tag>(name),
    m_context(context),
    m_log(context.log(name)),
    m_reactor(reactor),
    m_router(new router_t(*m_log.get()))
{
    on<io::locator::resolve>(std::bind(&locator_t::resolve, this, _1));
    on<io::locator::connect>(std::bind(&locator_t::connect, this, _1));
    on<io::locator::refresh>(std::bind(&locator_t::refresh, this, _1));

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

    if(root.as_object().count("discovery")) {
        const auto cluster_conf = root.as_object().at("discovery").as_object();
        const auto cluster_type = cluster_conf.at("type", "unspecified").as_string();
        const auto cluster_args = cluster_conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' for cluster discovery", cluster_type);

        m_cluster = m_context.get<api::cluster_t>(cluster_type, m_context, *this, name, cluster_args);
    }

    if(root.as_object().count("gateway")) {
        const auto gateway_conf = root.as_object().at("gateway").as_object();
        const auto gateway_type = gateway_conf.at("type", "unspecified").as_string();
        const auto gateway_args = gateway_conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' as a cluster accessor", gateway_type);

        m_gateway = m_context.get<api::gateway_t>(gateway_type, m_context, name, gateway_args);
    }
}

locator_t::~locator_t() {
    // Empty.
}

auto
locator_t::prototype() const -> const basic_dispatch_t& {
    return *this;
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
    public dispatch<io::event_traits<io::locator::connect>::upstream_type>
{
    locator_t& impl;
    const std::string uuid;

public:
    remote_client_t(locator_t& impl_, const std::string& uuid_):
        dispatch<io::event_traits<io::locator::connect>::upstream_type>(impl_.name()),
        impl(impl_),
        uuid(uuid_)
    {
        typedef io::protocol<
            io::event_traits<io::locator::connect>::upstream_type
        >::scope protocol;

        on<protocol::chunk>(std::bind(&remote_client_t::announce, this, _1));
        on<protocol::choke>(std::bind(&remote_client_t::shutdown, this));
    }

private:
    void
    announce(const result_of<io::locator::connect>::type& dump) {
        COCAINE_LOG_INFO(impl.m_log, "remote node has been updated")(
            "uuid", uuid
        );

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
        COCAINE_LOG_INFO(impl.m_log, "remote node has been shut down")(
            "uuid", uuid
        );

        auto removed = impl.m_router->remove_remote(uuid);

        for(auto it = removed.begin(); it != removed.end(); ++it) {
            impl.m_gateway->cleanup(uuid, it->first);
        }

        // NOTE: It is dangerous to disconnect the remote while the message is still being
        // processed, so we defer it via reactor_t::post().
        impl.m_reactor.post(deferred_erase_action<decltype(impl.m_remotes)>{impl.m_remotes, uuid});
    }
};

void
locator_t::link_node_impl(const std::string& uuid, const std::vector<boost::asio::ip::tcp::endpoint>& endpoints) {
    std::unique_ptr<io::channel<io::socket<io::tcp>>> channel;

    if(!m_gateway || m_remotes.find(uuid) != m_remotes.end()) {
        return;
    }

    for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        try {
            channel = std::make_unique<io::channel<io::socket<io::tcp>>>(
                m_reactor,
                std::make_shared<io::socket<io::tcp>>(*it)
            );
        } catch(const std::system_error& e) {
            continue;
        }

        COCAINE_LOG_DEBUG(m_log, "starting synchronization with remote node via %s", *it)(
            "uuid", uuid
        );

        break;
    }

    if(!channel) {
        std::ostringstream stream;
        std::ostream_iterator<boost::asio::ip::tcp::endpoint> builder(stream, ", ");

        std::copy(endpoints.begin(), endpoints.end(), builder);

        COCAINE_LOG_ERROR(m_log, "remote node is unreachable, tried: %s", endpoints.size(), stream.str())(
            "uuid", uuid
        );

        return;
    }

    channel->rd->bind(
        std::bind(&locator_t::on_message, this, uuid, std::placeholders::_1),
        std::bind(&locator_t::on_failure, this, uuid, std::placeholders::_1)
    );

    channel->wr->bind(
        std::bind(&locator_t::on_failure, this, uuid, std::placeholders::_1)
    );

    // Start the synchronization with a random UUID.

    auto service = std::make_shared<remote_client_t>(*this, uuid);

    m_remotes[uuid] = std::make_shared<session_t>(std::move(channel));
    m_remotes[uuid]->invoke(service)->send<io::locator::connect>(unique_id_t().string());
}

void
locator_t::drop_node_impl(const std::string& uuid) {
    if(!m_gateway || m_remotes.find(uuid) == m_remotes.end()) {
        return;
    }

    COCAINE_LOG_DEBUG(m_log, "stopping synchronization with remote node")(
        "uuid", uuid
    );

    auto removed = m_router->remove_remote(uuid);

    for(auto it = removed.begin(); it != removed.end(); ++it) {
        m_gateway->cleanup(uuid, it->first);
    }

    m_remotes[uuid]->detach();
    m_remotes.erase(uuid);
}

void
locator_t::link_node(const std::string& uuid, const std::vector<boost::asio::ip::tcp::endpoint>& endpoints) {
    m_reactor.post(std::bind(&locator_t::link_node_impl, this, uuid, endpoints));
}

void
locator_t::drop_node(const std::string& uuid) {
    m_reactor.post(std::bind(&locator_t::drop_node_impl, this, uuid));
}

auto
locator_t::resolve(const std::string& name) const -> resolve_result_t {
    auto basename = m_router->select_service(name);
    auto provided = m_context.locate(basename);

    if(provided) {
        COCAINE_LOG_DEBUG(m_log, "providing service using local node")(
            "service", name
        );

        // TODO: Might be a good idea to return an endpoint suitable for the interface which the
        // client used to connect to the Locator.
        return provided.get().metadata();
    }

    if(m_gateway) {
        return m_gateway->resolve(basename);
    } else {
        throw cocaine::error_t("service '%s' is not available", name);
    }
}

auto
locator_t::connect(const std::string& uuid) -> streamed<connect_result_t> {
    streamed<connect_result_t> stream;

    COCAINE_LOG_INFO(m_log, "connecting remote node into synchronization streams")(
        "uuid", uuid
    );

    if(!m_streams.insert({uuid, stream}).second) {
        stream.close();
    } else {
        stream.write(connect_result_t());
    }

    return stream;
}

auto
locator_t::refresh(const std::string& name) -> refresh_result_t {
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

void
locator_t::on_message(const std::string& uuid, const message_t& message) {
    auto it = m_remotes.find(uuid);

    if(it == m_remotes.end()) {
        return;
    }

    it->second->invoke(message);
}

void
locator_t::on_failure(const std::string& uuid, const std::error_code& ec) {
    if(m_remotes.find(uuid) == m_remotes.end()) {
        return;
    }

    if(ec) {
        COCAINE_LOG_WARNING(m_log, "remote node has unexpectedly disconnected: [%d] %s", ec.value(), ec.message())(
            "uuid", uuid
        );
    } else {
        COCAINE_LOG_WARNING(m_log, "remote node has unexpectedly disconnected")(
            "uuid", uuid
        );
    }

    drop_node_impl(uuid);
}
