/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/gateways/adhoc.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::api;
using namespace cocaine::gateway;

adhoc_t::adhoc_t(context_t& context, const std::string& name, const Json::Value& args):
    category_type(context, name, args),
    m_log(new logging::log_t(context, name))
{
#if defined(__clang__) || defined(HAVE_GCC46)
    std::random_device device;
    m_random_generator.seed(device());
#else
    m_random_generator.seed(static_cast<unsigned long>(::time(nullptr)));
#endif
}

adhoc_t::~adhoc_t() {
    // Empty.
}

auto
adhoc_t::resolve(const std::string& name) const -> metadata_t {
    auto it  = m_remote_services.cend(),
         end = it;

    std::tie(it, end) = m_remote_services.equal_range(name);

    if(it == end) {
        throw cocaine::error_t("the specified service is not available in the group");
    }

#if defined(__clang__) || defined(HAVE_GCC46)
    std::uniform_int_distribution<int> distribution(0, std::distance(it, end) - 1);
#else
    std::uniform_int<int> distribution(0, std::distance(it, end) - 1);
#endif

    std::advance(it, distribution(m_random_generator));

    const auto endpoint = std::get<0>(it->second.meta);

    COCAINE_LOG_DEBUG(
        m_log,
        "providing '%s' using remote node '%s' on %s:%d",
        name,
        it->second.uuid,
        std::get<0>(endpoint),
        std::get<1>(endpoint)
    );

    return it->second.meta;
}

void
adhoc_t::consume(const std::string& uuid, const std::string& name, const metadata_t& meta) {
    COCAINE_LOG_DEBUG(m_log, "consumed node '%s' service '%s'", uuid, name);

    m_remote_services.insert({
        name,
        remote_service_t { uuid, meta }
    });
}

void
adhoc_t::cleanup(const std::string& uuid, const std::string& name) {
    COCAINE_LOG_DEBUG(m_log, "removing node '%s' service '%s'", uuid, name);

    remote_service_map_t::iterator it, end;

    std::tie(it, end) = m_remote_services.equal_range(name);

    while(it != end) {
        if(it->second.uuid == uuid) {
            m_remote_services.erase(it++);
        } else {
            ++it;
        }
    }
}
