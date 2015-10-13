/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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
#include "cocaine/logging.hpp"

using namespace cocaine::gateway;

adhoc_t::adhoc_t(context_t& context, const std::string& name, const dynamic_t& args):
    category_type(context, name, args),
    m_log(context.log(name))
{
    std::random_device rd; m_random_generator.seed(rd());
}

adhoc_t::~adhoc_t() {
    // Empty.
}

auto
adhoc_t::resolve(const partition_t& name) const -> std::vector<asio::ip::tcp::endpoint> {
    remote_map_t::const_iterator lb, ub;

    auto ptr = m_remotes.synchronize();

    if(!ptr->count(name)) {
        throw std::system_error(error::service_not_available);
    }

    std::tie(lb, ub) = ptr->equal_range(name);

    std::uniform_int_distribution<int> distribution(0, std::distance(lb, ub) - 1);
    std::advance(lb, distribution(m_random_generator));

    COCAINE_LOG_DEBUG(m_log, "providing service using remote actor")(
        "uuid", lb->second.uuid
    );

    return lb->second.endpoints;
}

size_t
adhoc_t::consume(const std::string& uuid,
                 const partition_t& name, const std::vector<asio::ip::tcp::endpoint>& endpoints)
{
    auto ptr = m_remotes.synchronize();

    ptr->insert({
        name,
        remote_t{uuid, endpoints}
    });

    COCAINE_LOG_DEBUG(m_log, "registering destination with %d endpoints", endpoints.size())(
        "service", std::get<0>(name),
        "uuid", uuid,
        "version", std::get<1>(name)
    );

    return ptr->count(name);
}

size_t
adhoc_t::cleanup(const std::string& uuid, const partition_t& name) {
    remote_map_t::const_iterator lb, ub;

    auto ptr = m_remotes.synchronize();

    // Narrow search to the specified service partition.
    std::tie(lb, ub) = ptr->equal_range(name);

    // Since UUIDs are unique, only one remote will match the specified UUID.
    auto it = std::find_if(lb, ub, [&](const remote_map_t::value_type& value) -> bool {
        return value.second.uuid == uuid;
    });

    COCAINE_LOG_DEBUG(m_log, "removing destination with %d endpoints", it->second.endpoints.size())(
        "service", std::get<0>(name),
        "uuid", uuid,
        "version", std::get<1>(name)
    );

    ptr->erase(it); return ptr->count(name);
}
