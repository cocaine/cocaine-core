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
adhoc_t::resolve(const std::string& name) const -> metadata_t {
    remote_service_map_t::const_iterator lb, ub;

    if(!m_remote_services.count(name)) {
        throw boost::system::system_error(error::service_not_available);
    }

    std::tie(lb, ub) = m_remote_services.equal_range(name);

    std::uniform_int_distribution<int> distribution(0, std::distance(lb, ub) - 1);
    std::advance(lb, distribution(m_random_generator));

    COCAINE_LOG_DEBUG(m_log, "providing service using remote node")(
        "service", name,
        "uuid",    lb->second.uuid
    );

    return lb->second.info;
}

void
adhoc_t::consume(const std::string& uuid, const std::string& name, const metadata_t& info) {
    m_remote_services.insert({
        name,
        remote_service_t{uuid, info}
    });

    COCAINE_LOG_DEBUG(m_log, "adding '%s' backend %s", name, uuid);
}

void
adhoc_t::cleanup(const std::string& uuid, const std::string& name) {
    remote_service_map_t::iterator it, end;

    // Only one remote will match the specified arguments.
    std::tie(it, end) = m_remote_services.equal_range(name);

    while(it != end) {
        if(it->second.uuid != uuid) {
            it++;
        } else {
            it = m_remote_services.erase(it);
        }
    }

    COCAINE_LOG_DEBUG(m_log, "erased '%s' backend %s", name, uuid);
}
