/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_GATEWAY_API_HPP
#define COCAINE_GATEWAY_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/rpc/graph.hpp"

#include <asio/ip/tcp.hpp>

namespace cocaine { namespace api {

struct gateway_t {
    typedef gateway_t category_type;

    struct service_description_t {
        std::vector<asio::ip::tcp::endpoint> endpoints;
        io::graph_root_t protocol;
        unsigned int version;
    };

    enum class resolve_policy_t {
        full,
        remote_only
    };

    virtual
    ~gateway_t() {
     // Empty.
    }

    /**
     * This one tells the locator (or any other direct user of gateway) if the gateway resolves
     * local services itself("full") or delegates resolving of local services to locator("remote_only").
     */
    virtual
    auto
    resolve_policy() const -> resolve_policy_t = 0;

    virtual
    auto
    resolve(const std::string& name) const -> service_description_t = 0;

    /**
     * this is called when locator discovers new service (remote or local)
     * local services can be distinguished by local_uuid parameter passed to ctor
     */
    virtual
    auto
    consume(const std::string& uuid,
            const std::string& name,
            unsigned int version,
            const std::vector<asio::ip::tcp::endpoint>& endpoints,
            const io::graph_root_t& protocol) -> void = 0;


    /**
     * cleanup only one specific service from concrete uuid
     */
    virtual
    auto
    cleanup(const std::string& uuid, const std::string& name) -> void = 0;

    /**
     * drop all services from uuid, typically it's called on node shutdown or disconnect
     */
    virtual
    auto
    cleanup(const std::string& uuid) -> void = 0;

    /**
     * count all services with specified name including local ones
     */
    virtual
    auto
    total_count(const std::string& name) const -> size_t = 0;

protected:
    gateway_t(context_t&, const std::string& /* local_uuid */, const std::string& /* name */, const dynamic_t& /* args */) {
        // Empty.
    }
};

typedef std::unique_ptr<gateway_t> gateway_ptr;

}} // namespace cocaine::api

#endif
