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

#include "cocaine/common.hpp"

#include "cocaine/api/cluster.hpp"
#include "cocaine/api/service.hpp"

#include "cocaine/idl/locator.hpp"
#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/result_of.hpp"

namespace cocaine {

class session_t;

}

namespace cocaine { namespace service {

class locator_t:
    public api::service_t,
    public api::cluster_t::interface,
    public dispatch<io::locator_tag>
{
    class remote_client_t;
    class router_t;

    context_t& m_context;

    const std::unique_ptr<logging::log_t> m_log;

    // Cluster interconnections.
    io::reactor_t& m_reactor;

    typedef result_of<io::locator::resolve>::type resolve_result_t;
    typedef result_of<io::locator::connect>::type connect_result_t;
    typedef result_of<io::locator::refresh>::type refresh_result_t;

    // These are remote sessions indexed by uuid. The uuid is required to easily disambiguate between
    // different runtime instances on the same host.
    std::map<std::string, std::shared_ptr<session_t>> m_remotes;
    std::map<std::string, streamed<connect_result_t>> m_streams;

    // Remoting.
    std::unique_ptr<api::gateway_t> m_gateway;
    std::unique_ptr<api::cluster_t> m_cluster;

    // Used to resolve service names against service groups based on weights and other metrics.
    std::unique_ptr<router_t> m_router;

public:
    locator_t(context_t& context, io::reactor_t& reactor, const std::string& name, const dynamic_t& args);

    virtual
   ~locator_t();

    virtual
    auto
    prototype() -> io::basic_dispatch_t&;

    // Cartel API

    virtual
    void
    link_node(const std::string& uuid, const std::vector<boost::asio::ip::tcp::endpoint>& endpoints);

    virtual
    void
    drop_node(const std::string& uuid);

private:
    auto
    resolve(const std::string& name) const -> resolve_result_t;

    auto
    connect(const std::string& node) -> streamed<connect_result_t>;

    auto
    refresh(const std::string& name) -> refresh_result_t;

    // Synchronization

    void
    on_message(const std::string& node, const io::message_t& message);

    void
    on_failure(const std::string& node, const std::error_code& ec);
};

}} // namespace cocaine::service

#endif
