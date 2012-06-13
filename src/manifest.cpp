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

    policy.termination_timeout = root["engine"].get(
        "termination-timeout",
        defaults::termination_timeout
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
