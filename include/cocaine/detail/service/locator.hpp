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

#ifndef COCAINE_LOCATOR_SERVICE_HPP
#define COCAINE_LOCATOR_SERVICE_HPP

#include "cocaine/api/cluster.hpp"
#include "cocaine/api/service.hpp"

#include "cocaine/detail/service/locator/routing.hpp"

#include "cocaine/idl/context.hpp"
#include "cocaine/idl/locator.hpp"

#include "cocaine/rpc/dispatch.hpp"

#include "cocaine/locked_ptr.hpp"

namespace cocaine { namespace service {

class locator_t;

namespace results {

typedef result_of<io::locator::resolve>::type resolve;
typedef result_of<io::locator::connect>::type connect;
typedef result_of<io::locator::cluster>::type cluster;
typedef result_of<io::locator::routing>::type routing;

} // namespace results

class locator_cfg_t
{
public:
    locator_cfg_t(const std::string& name, const dynamic_t& args);

    std::string name;
    std::string uuid;

    // Restricted services.
    std::set<std::string> restricted;
};

class locator_t:
    public api::service_t,
    public api::cluster_t::interface,
    public dispatch<io::locator_tag>
{
    class connect_sink_t;
    class publish_slot_t;
    class routing_slot_t;

    typedef std::map<std::string, continuum_t> rg_map_t;

    class uplink_t
    {
    public:
        std::vector<asio::ip::tcp::endpoint> endpoints;
        std::shared_ptr<session<asio::ip::tcp>> ptr;
    };

    typedef std::map<std::string, uplink_t> client_map_t;

    typedef std::map<std::string, streamed<results::connect>> remote_map_t;
    typedef std::map<std::string, streamed<results::routing>> router_map_t;

    context_t& m_context;

    const std::unique_ptr<logging::logger_t> m_log;
    const locator_cfg_t m_cfg;

    // Cluster interconnections.
    asio::io_service& m_asio;

    // Slot for context signals.
    std::shared_ptr<dispatch<io::context_tag>> m_signals;

    // Clustering components.
    std::unique_ptr<api::cluster_t> m_cluster;
    std::unique_ptr<api::gateway_t> m_gateway;

    // Used to resolve service names against routing groups, based on weights and other metrics.
    synchronized<rg_map_t> m_rgs;

    // Incoming remote locator streams indexed by uuid. Uuid is required to disambiguate between
    // multiple different instances on the same host and port (in case it was restarted).
    synchronized<client_map_t> m_clients;

    // Outgoing remote locator streams indexed by node uuid.
    synchronized<remote_map_t> m_remotes;

    // Snapshots of the local service states. Synchronized with outgoing remote streams.
    std::map<std::string, results::resolve> m_snapshots;

    // Outgoing router streams indexed by some arbitrary router-provided uuid.
    synchronized<router_map_t> m_routers;

    std::uint32_t link_attempts;
    synchronized<std::unique_ptr<asio::deadline_timer>> link_timer;

public:
    locator_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    virtual
   ~locator_t();

    // Service API

    virtual
    auto
    prototype() -> io::basic_dispatch_t&;

    // Cluster API

    virtual
    auto
    asio() -> asio::io_service&;

    virtual
    void
    link_node(const std::string& uuid, const std::vector<asio::ip::tcp::endpoint>& endpoints);

    virtual
    void
    drop_node(const std::string& uuid);

    virtual
    std::string
    uuid() const;

private:
    auto
    retry_link_node(const std::string& uuid, const std::vector<asio::ip::tcp::endpoint>& endpoints) -> void;

    auto
    on_resolve(const std::string& name, const std::string& seed) const -> results::resolve;

    auto
    on_connect(const std::string& uuid) -> streamed<results::connect>;

    void
    on_refresh(const std::vector<std::string>& groups);

    auto
    on_cluster() const -> results::cluster;

    auto
    on_routing(const std::string& ruid, bool replace = false) -> streamed<results::routing>;

    // Context signals

    enum class modes { exposed, removed };

    void
    on_service(const std::string& name, const results::resolve& meta, modes mode);

    void
    on_context_shutdown();
};

}} // namespace cocaine::service

#endif
