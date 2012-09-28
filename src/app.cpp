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

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>

#include "cocaine/app.hpp"

#include "cocaine/archive.hpp"
#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::engine;

namespace fs = boost::filesystem;

app_t::app_t(context_t& context, const std::string& name, const std::string& profile):
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % name
        ).str()
    )),
    m_manifest(context, name),
    m_profile(context, profile)
{
    fs::path path(
        fs::path(m_context.config.spool_path) / name
    );
    
    // Checking if the app files are deployed.
    if(!fs::exists(path)) {
        deploy(name, path.string());
    }

    // NOTE: The event loop is not started here yet.
    m_engine.reset(
        new engine_t(
            m_context,
            m_manifest,
            m_profile
        )
    );

    // Initialize the drivers
    // ----------------------

    Json::Value drivers(m_manifest.drivers);

    if(drivers.empty()) {
        return;
    }
    
    Json::Value::Members names(drivers.getMemberNames());

    m_log->info(
        "initializing %zu %s: %s",
        drivers.size(),
        drivers.size() == 1 ? "driver" : "drivers",
        boost::algorithm::join(names, ", ").c_str()
    );

    boost::format format("%s/%s");

    for(Json::Value::Members::iterator it = names.begin();
        it != names.end();
        ++it)
    {
        try {
            format % name % *it;

            m_drivers.insert(
                *it,
                m_context.get<api::driver_t>(
                    drivers[*it]["type"].asString(),
                    api::category_traits<api::driver_t>::args_type(
                        *m_engine,
                        format.str(),
                        drivers[*it]
                    )
                )
            );
        } catch(const std::exception& e) {
            m_log->error(
                "unable to initialize the '%s' driver - %s",
                format.str().c_str(),
                e.what()
            );

            boost::format message("unable to initialize the drivers");
            throw configuration_error_t((message % name).str());
        }

        format.clear();
    }
}

app_t::~app_t() {
    // NOTE: Stop the engine first, so that there won't be any
    // new events during the drivers shutdown process.
    stop();
}

void app_t::start() {
    m_engine->start();
}

void app_t::stop() {
    m_engine->stop();
}

Json::Value app_t::info() const {
    Json::Value info(m_engine->info());

    for(driver_map_t::const_iterator it = m_drivers.begin();
        it != m_drivers.end();
        ++it) 
    {
        info["drivers"][it->first] = it->second->info();
    }

    return info;
}

bool app_t::enqueue(const boost::shared_ptr<job_t>& job, mode::value mode) {
    return m_engine->enqueue(job, mode);
}

void app_t::deploy(const std::string& name, const std::string& path) {
    std::string blob;

    m_log->info(
        "deploying the app to '%s'",
        path.c_str()
    );
    
    api::category_traits<api::storage_t>::ptr_type storage(
        m_context.get<api::storage_t>("storage/core")
    );
    
    try {
        blob = storage->get<std::string>("apps", name);
    } catch(const storage_error_t& e) {
        m_log->error("unable to fetch the app from the storage - %s", e.what());
        boost::format message("the '%s' app is not available");
        throw configuration_error_t((message % name).str());
    }
    
    try {
        archive_t archive(m_context, blob);
        archive.deploy(path);
    } catch(const archive_error_t& e) {
        m_log->error("unable to extract the app files - %s", e.what());
        boost::format message("the '%s' app is not available");
        throw configuration_error_t((message % name).str());
    }
}
