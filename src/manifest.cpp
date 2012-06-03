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

#include "cocaine/manifest.hpp"

#include "cocaine/context.hpp"

#include "cocaine/interfaces/storage.hpp"

using namespace cocaine::engine;
using namespace cocaine::storages;

// Application
// -----------

manifest_t::manifest_t(context_t& context, const std::string& app):
    name(app)
{
    boost::shared_ptr<storage_t> storage(
        context.storage("core")
    );

    if(!storage->exists("apps", name)) {
        throw configuration_error_t("the specified app is not available");
    }

    // Load the app manifest.
    root = storage->get("apps", name);

    // Setup the app configuration.
    type = root["type"].asString();

    // Setup the engine policies.
    policy.heartbeat_timeout = root["engine"].get(
        "heartbeat-timeout",
        defaults::heartbeat_timeout
    ).asDouble();

    policy.suicide_timeout = root["engine"].get(
        "suicide-timeout",
        defaults::suicide_timeout
    ).asDouble();
    
    policy.pool_limit = root["engine"].get(
        "pool-limit",
        defaults::pool_limit
    ).asUInt();
    
    policy.queue_limit = root["engine"].get(
        "queue-limit",
        defaults::queue_limit
    ).asUInt();

    policy.grow_threshold = root["engine"].get(
        "grow-threshold",
        policy.queue_limit / policy.pool_limit
    ).asUInt();

    slave = root.get(
        "slave",
        defaults::slave
    ).asString();
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
