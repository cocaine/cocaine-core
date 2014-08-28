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

#ifndef COCAINE_SERVICE_LOCATOR_HPP
#define COCAINE_SERVICE_LOCATOR_HPP

#include "cocaine/api/cluster.hpp"
#include "cocaine/api/resolve.hpp"
#include "cocaine/api/service.hpp"

#include "cocaine/idl/locator.hpp"
#include "cocaine/rpc/dispatch.hpp"

#include "cocaine/locked_ptr.hpp"

namespace cocaine {

class actor_t;

} // namespace cocaine

namespace cocaine { namespace service {

class locator_t;

namespace results {
    typedef result_of<io::locator::resolve>::type resolve;
    typedef result_of<io::locator::connect>::type connect;
    typedef result_of<io::locator::cluster>::type cluster;
}

class locator_t:
    public api::service_t,
    public api::cluster_t::interface,
    public dispatch<io::locator_tag>
{
    class remote_client_t;
    class router_t;

    class cleanup_action_t;

    context_t& m_context;

    const std::unique_ptr<logging::log_t> m_log;

    // Cluster interconnections.
    boost::asio::io_service& m_asio;

    // Node UUID.
    const std::string m_uuid;

    // Remote sessions are created using this resolve.
    std::unique_ptr<api::resolve_t> m_resolve;

    // Remote sessions indexed by uuid. The uuid is required to disambiguate between different
    // instances on the same host, even if the instance was restarted on the same port.
    std::map<std::string, std::shared_ptr<api::client<io::locator_tag>>> m_remotes;

    struct locals_t {
        results::connect snapshot;

        // Outgoing synchronization streams.
        std::map<std::string, streamed<results::connect>> streams;
    };

    // Local services state.
    synchronized<locals_t> m_locals;

    // Clustering.
    std::unique_ptr<api::gateway_t> m_gateway;
    std::unique_ptr<api::cluster_t> m_cluster;

    // Used to resolve service names against service groups based on weights and other metrics.
    std::unique_ptr<router_t> m_routing;

public:
    locator_t(context_t& context, boost::asio::io_service& asio, const std::string& name, const dynamic_t& args);

    virtual
   ~locator_t();

    // Service API

    virtual
    auto
    prototype() const -> const io::basic_dispatch_t&;

    // Cluster API

    virtual
    boost::asio::io_service&
    asio();

    virtual
    void
    link_node(const std::string& uuid, const std::vector<boost::asio::ip::tcp::endpoint>& endpoints);

    virtual
    void
    drop_node(const std::string& uuid);

    virtual
    std::string
    uuid() const;

private:
    auto
    on_resolve(const std::string& name) const -> results::resolve;

    auto
    on_connect(const std::string& uuid) -> streamed<results::connect>;

    void
    on_refresh(const std::string& name);

    auto
    on_cluster() const -> results::cluster;

    // Context signals

    void
    on_service(const actor_t& actor);

    void
    on_context_shutdown();
};

}} // namespace cocaine::service

#endif
