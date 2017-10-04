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

#include "cocaine/detail/gateway/adhoc.hpp"

#include "cocaine/context.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/format/optional.hpp"
#include "cocaine/format/header.hpp"
#include "cocaine/hpack/header.hpp"
#include "cocaine/logging.hpp"

#include <blackhole/logger.hpp>
#include <cocaine/rpc/graph.hpp>

#include <boost/optional/optional.hpp>

namespace cocaine {
namespace gateway {

adhoc_t::adhoc_t(context_t& context, const std::string& _local_uuid, const std::string& name, const dynamic_t& args,
                 const dynamic_t::object_t& locator_extra):
    category_type(context, _local_uuid, name, args, locator_extra),
    m_log(context.log(name)),
    x_cocaine_cluster(locator_extra.at("x-cocaine-cluster", "").as_string())
{
    std::random_device rd;
    m_random_generator.seed(rd());
}

auto
adhoc_t::resolve(const std::string& name) const -> service_description_t {
    return m_remotes.apply([&](const remote_map_t& remotes) {
        auto by_service_it = remotes.find(name);
        if(by_service_it == remotes.end() || by_service_it->second.empty()) {
            throw std::system_error(error::service_not_available);
        }

        // roll the dice and choose random one
        auto& services_by_uuid = by_service_it->second;
        auto it = services_by_uuid.begin();
        std::uniform_int_distribution<int> distribution(0, services_by_uuid.size() - 1);
        std::advance(it, distribution(m_random_generator));

        COCAINE_LOG_DEBUG(m_log, "providing service using remote actor", blackhole::attribute_list({
            {"uuid", it->second.uuid}
        }));

        return service_description_t{it->second.endpoints, it->second.protocol, it->second.version};
    });
}

auto
adhoc_t::consume(const std::string& uuid,
                 const std::string& name,
                 unsigned int version,
                 const std::vector<asio::ip::tcp::endpoint>& endpoints,
                 const io::graph_root_t& protocol,
                 const dynamic_t::object_t& extra) -> void
{
    auto cluster = extra.at("x-cocaine-cluster", "").as_string();
    if(cluster != x_cocaine_cluster) {
        COCAINE_LOG_INFO(m_log, "skipping service {} from {} due to different x-cocaine-cluster extra param - {}, required {}",
                         name, uuid, cluster, x_cocaine_cluster);
        return;
    }
    m_remotes.apply([&](remote_map_t& remotes){
        bool inserted;
        std::tie(std::ignore, inserted) = remotes[name].insert({uuid, remote_t{uuid, version, endpoints, protocol}});

        if(!inserted) {
            throw error_t(error::gateway_duplicate_service,
                          "failed to add remote service {} located on {} to gateway: service already registered",
                          name, uuid);
        } else {
            COCAINE_LOG_INFO(m_log, "registered {}/{} destination with {:d} endpoints from {}",
                              name, version, endpoints.size(), uuid);
        }

    });
}

auto
adhoc_t::cleanup(const std::string& uuid, const std::string& name) -> void {
    m_remotes.apply([&](remote_map_t& remotes){
        if(remotes[name].erase(uuid)) {
            COCAINE_LOG_INFO(m_log, "removed service {} provided by {} from gateway", name, uuid);
        } else {
            throw error_t(error::gateway_missing_service,
                          "failed to remove service {} provided by {} from gateway: not found", name, uuid);
        }
    });
}

auto
adhoc_t::cleanup(const std::string& uuid) -> void {
    m_remotes.apply([&](remote_map_t& remotes){
        size_t removed = 0;
        for(auto it = remotes.begin(); it != remotes.end();) {
            removed += it->second.erase(uuid);
            if(it->second.empty()) {
                it = remotes.erase(it);
            } else {
                it++;
            }
        }
        COCAINE_LOG_INFO(m_log, "removed {} services from {} remote", removed, uuid);
    });
}

auto
adhoc_t::total_count(const std::string& name) const -> size_t {
    return m_remotes.apply([&](const remote_map_t& remotes) -> size_t {
        const auto it = remotes.find(name);
        if(it == remotes.end()) {
            return 0ul;
        }
        return it->second.size();
    });
}

} // namespace gateway
} // namespace cocaine
