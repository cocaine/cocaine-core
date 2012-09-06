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

#include "cocaine/archive.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/api/storage.hpp"

using namespace cocaine;

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
        root = m_context.get<api::storage_t>("storage/cache")->get<Json::Value>("manifests", name);
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

    if(policy.grow_threshold == 0) {
        throw configuration_error_t("engine grow threshold must be positive");
    }

    slave = root["engine"].get(
        "slave",
        defaults::slave
    ).asString();
}

void manifest_t::deploy() {
    std::string blob;

    api::category_traits<api::storage_t>::ptr_type storage(
        m_context.get<api::storage_t>("storage/core")
    );
    
    try {
        // Fetch the application manifest and archive from the core storage.
        root = storage->get<Json::Value>("manifests", name);
        blob = storage->get<std::string>("apps", name);
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
        archive_t archive(m_context, blob);
        archive.deploy(path);
    } catch(const archive_error_t& e) {
        m_log->error("unable to extract the app - %s", e.what());
        throw configuration_error_t("the '" + name + "' app is not available");
    }

    // Update the manifest in the cache.
    root["path"] = path;

    try {
        // Put the application object into the cache for future reference.
        m_context.get<api::storage_t>("storage/cache")->put("manifests", name, root);
    } catch(const storage_error_t& e) {
        m_log->warning("unable to cache the app manifest - %s", e.what());
        throw configuration_error_t("the '" + name + "' app is not available");
    }    
}
