//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>

#include "cocaine/app.hpp"

#include "cocaine/context.hpp"

#include "cocaine/interfaces/storage.hpp"

using namespace cocaine::engine;
using namespace cocaine::storages;

// Application
// -----------

app_t::app_t(context_t& context, const std::string& name):
    m_name(name),
    log(context.log(name))
{
    boost::shared_ptr<storage_t> storage(
        context.storage("core")
    );

    if(!storage->exists("apps", m_name)) {
        throw configuration_error_t("the specified app is not available");
    }

    // Load the application manifest.
    m_manifest = storage->get("apps", m_name);

    // Setup the app configuration.
    m_type = m_manifest["type"].asString();

    // Setup the engine policies.
    policy.heartbeat_timeout = m_manifest["engine"].get(
        "heartbeat-timeout",
        defaults::heartbeat_timeout
    ).asDouble();

    policy.suicide_timeout = m_manifest["engine"].get(
        "suicide-timeout",
        defaults::suicide_timeout
    ).asDouble();
    
    policy.pool_limit = m_manifest["engine"].get(
        "pool-limit",
        defaults::pool_limit
    ).asUInt();
    
    policy.queue_limit = m_manifest["engine"].get(
        "queue-limit",
        defaults::queue_limit
    ).asUInt();

    policy.grow_threshold = m_manifest["engine"].get(
        "grow-threshold",
        policy.queue_limit / policy.pool_limit
    ).asUInt();
}

endpoint::endpoint(const std::string& name) {
    m_endpoint = boost::algorithm::join(
        boost::assign::list_of
            (std::string("ipc:///var/run/cocaine"))
            (name),
        "/");
}

endpoint::operator std::string() const {
    return m_endpoint;
}
