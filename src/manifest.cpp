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

#include "cocaine/manifest.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/package.hpp"

#include "cocaine/interfaces/storage.hpp"

using namespace cocaine;
using namespace cocaine::storages;

manifest_t::manifest_t(context_t& context, const std::string& name_):
    name(name_),
    m_log(context.log("app/" + name))
{
    try {
        // Load the app manifest.
        root = context.storage<objects>("core:cache")->exists("apps", name);
        spool_path = root["spool"].asString();
    } catch(const storage_error_t& e) {
        m_log->info("the app hasn't been found in the cache");

        objects::value_type object;

        try {
            // Fetch the application object from the core storage.
            object = context.storage<objects>("core")->get("apps", name);
        } catch(const storage_error_t& e) {
            m_log->error("unable to fetch the app from the storage - %s", e.what());
            throw configuration_error_t("the '" + name + "' app is not available");
        }

        // Unpack the app.
        spool_path = context.config.spool_path / name;
        
        m_log->info(
            "deploying the app to '%s'",
            spool_path.string().c_str()
        );
        
        try {
            package_t package(context, object.blob);
            package.deploy(spool_path); 
        } catch(const package_error_t& e) {
            m_log->error("unable to deploy the app - %s", e.what());
            throw configuration_error_t("the '" + name + "' app is not available");
        }

        // Update the manifest in the cache.
        object.meta["spool"] = spool_path.string();
        root = object.meta;

        try {
            // Put the application object into the cache for future reference.
            context.storage<objects>("core:cache")->put("apps", name, object);
        } catch(const storage_error_t& e) {
            m_log->warning("unable to cache the app - %s", e.what());
        }
    }

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

    slave = root["engine"].get(
        "slave",
        defaults::slave
    ).asString();
}
