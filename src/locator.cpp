/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/locator.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/detail/actor.hpp"

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::logging;

using namespace std::placeholders;

locator_t::locator_t(context_t& context):
    dispatch_t(context, "service/locator"),
    m_log(new log_t(context, "service/locator"))
{
    on<locator::resolve>("resolve", std::bind(&locator_t::resolve, this, _1));
}

locator_t::~locator_t() {
    for(service_list_t::reverse_iterator it = m_services.rbegin();
        it != m_services.rend();
        ++it)
    {
        COCAINE_LOG_INFO(m_log, "stopping service '%s'", it->first);

        // Terminate the service's thread.
        it->second->terminate();
    }

    m_services.clear();
}

void
locator_t::attach(const std::string& name,
                  std::unique_ptr<actor_t>&& service)
{
    COCAINE_LOG_INFO(
        m_log,
        "publishing service '%s' on '%s'",
        name,
        service->endpoint()
    );

    // Start the service's thread.
    service->run();

    m_services.emplace_back(
        name,
        std::move(service)
    );
}

namespace {
    struct match_t {
        match_t(const std::string& name):
            m_name(name)
        { }

        template<class T>
        bool
        operator()(const T& service) {
            return m_name == service.first;
        }

    private:
        const std::string m_name;
    };
}

locator::description_t
locator_t::resolve(const std::string& name) const {
    auto it = std::find_if(
        m_services.begin(),
        m_services.end(),
        match_t(name)
    );

    if(it == m_services.end()) {
        throw cocaine::error_t("the specified service is not available");
    }

    locator::description_t result = {
        it->second->endpoint(),
        1,
        it->second->dispatch().describe()
    };

    return result;
}
