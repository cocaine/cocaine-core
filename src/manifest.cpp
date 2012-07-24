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

#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>

#include "cocaine/manifest.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/package.hpp"

#include "cocaine/interfaces/storage.hpp"

using namespace cocaine;
using namespace cocaine::storages;

manifest_t::manifest_t(context_t& context, const std::string& name_):
    name(name_),
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % name_
        ).str()
    ))
{
    try {
        // Try to load the app manifest from the cache.
        root = m_context.storage<objects>("core:cache")->exists("apps", name);
        path = root["path"].asString();
    } catch(const storage_error_t& e) {
        m_log->info("the app hasn't been found in the app cache");
        deploy();
    }

    // Setup the app configuration.
    type = root["type"].asString();

    // Setup the engine policies.
    policy.startup_timeout = root["engine"].get(
        "startup-timeout",
        defaults::startup_timeout
    ).asDouble();

    if(policy.startup_timeout <= 0.0f) {
        throw configuration_error_t("slave startup timeout must be positive");
    }
    
    policy.heartbeat_timeout = root["engine"].get(
        "heartbeat-timeout",
        defaults::heartbeat_timeout
    ).asDouble();

    if(policy.heartbeat_timeout <= 0.0f) {
        throw configuration_error_t("slave heartbeat timeout must be positive");
    }

    policy.idle_timeout = root["engine"].get(
        "idle-timeout",
        defaults::idle_timeout
    ).asDouble();

    if(policy.idle_timeout <= 0.0f) {
        throw configuration_error_t("slave idle timeout must be positive");
    }

    policy.termination_timeout = root["engine"].get(
        "termination-timeout",
        defaults::termination_timeout
    ).asDouble();
        
    policy.pool_limit = root["engine"].get(
        "pool-limit",
        static_cast<Json::UInt>(defaults::pool_limit)
    ).asUInt();

    if(policy.pool_limit == 0) {
        throw configuration_error_t("engine pool limit must be positive");
    }

    policy.queue_limit = root["engine"].get(
        "queue-limit",
        static_cast<Json::UInt>(defaults::queue_limit)
    ).asUInt();

    policy.grow_threshold = root["engine"].get(
        "grow-threshold",
        static_cast<Json::UInt>(policy.queue_limit / policy.pool_limit)
    ).asUInt();

    slave = root["engine"].get(
        "slave",
        defaults::slave
    ).asString();
}

void manifest_t::deploy() {
    objects::value_type object;

    try {
        // Fetch the application object from the core storage.
        object = m_context.storage<objects>("core")->get("apps", name);
    } catch(const storage_error_t& e) {
        m_log->error("unable to fetch the app from the app storage - %s", e.what());
        throw configuration_error_t("the '" + name + "' app is not available");
    }

    // Unpack the app.
    path = (boost::filesystem::path(m_context.config.spool_path) / name).string();
    
    m_log->info(
        "deploying the app to '%s'",
        path.c_str()
    );
    
    try {
        package_t package(m_context, object.blob);
        package.deploy(path); 
    } catch(const package_error_t& e) {
        m_log->error("unable to extract the app - %s", e.what());
        throw configuration_error_t("the '" + name + "' app is not available");
    }

    // Update the manifest in the cache.
    object.meta["path"] = path;
    root = object.meta;

    try {
        // Put the application object into the cache for future reference.
        m_context.storage<objects>("core:cache")->put("apps", name, object);
    } catch(const storage_error_t& e) {
        m_log->warning("unable to cache the app - %s", e.what());
        throw configuration_error_t("the '" + name + "' app is not available");
    }    
}
