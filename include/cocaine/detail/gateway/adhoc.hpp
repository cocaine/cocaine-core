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

#ifndef COCAINE_ADHOC_GATEWAY_HPP
#define COCAINE_ADHOC_GATEWAY_HPP

#include "cocaine/api/gateway.hpp"
#include "cocaine/locked_ptr.hpp"

#include <random>

namespace cocaine { namespace gateway {

class adhoc_t:
    public api::gateway_t
{
    const std::unique_ptr<logging::logger_t> m_log;

    // Used in resolve() method, which is const.
    std::default_random_engine mutable m_random_generator;

    struct remote_t {
        std::string uuid;
        unsigned int version;
        std::vector<asio::ip::tcp::endpoint> endpoints;
        io::graph_root_t protocol;
    };

    typedef std::map<std::string, std::map<std::string, remote_t>> remote_map_t;

    // TODO: Make sure that remote service metadata is consistent across the whole cluster.
    synchronized<remote_map_t> m_remotes;

public:
    adhoc_t(context_t& context, const std::string& _local_uuid, const std::string& name, const dynamic_t& args);

    auto
    resolve_policy() const ->resolve_policy_t override {
        return resolve_policy_t::remote_only;
    }

    auto
    resolve(const std::string& name) const -> service_description_t override;

    auto
    consume(const std::string& uuid,
            const std::string& name,
            unsigned int version,
            const std::vector<asio::ip::tcp::endpoint>& endpoints,
            const io::graph_root_t& protocol) -> void override;

    auto
    cleanup(const std::string& uuid, const std::string& name) -> void override;

    auto
    cleanup(const std::string& uuid) -> void override;

    auto
    total_count(const std::string& name) const -> size_t override;

};

}} // namespace cocaine::gateway

#endif
