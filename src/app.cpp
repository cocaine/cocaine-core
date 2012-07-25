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
#include <boost/format.hpp>

#include "cocaine/app.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::storages;

app_t::app_t(context_t& context, const std::string& name):
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % name
        ).str()
    )),
    m_manifest(context, name),
    m_engine(new engine_t(context, m_manifest))
{
    Json::Value drivers(m_manifest.root["drivers"]);

    if(drivers.isNull() || !drivers.size()) {
        return;
    }
    
    Json::Value::Members names(drivers.getMemberNames());

    m_log->info(
        "initializing %zu %s: %s",
        drivers.size(),
        drivers.size() == 1 ? "driver" : "drivers",
        boost::algorithm::join(names, ", ").c_str()
    );

    for(Json::Value::Members::iterator it = names.begin();
        it != names.end();
        ++it)
    {
        drivers::driver_config_t config = {
            *it,
            drivers[*it]
        };

        m_drivers.insert(
            *it,
            context.get<drivers::driver_t>(
                config.args["type"].asString(),
                category_traits<drivers::driver_t>::args_type(
                    *m_engine,
                    config
                )
            )
        );
    }
}

app_t::~app_t() {
    // NOTE: Stop the engine, then stop the drivers, so that
    // the pending jobs would still have their drivers available
    // to process the outstanding results.
    m_engine->stop();
    m_drivers.clear();

    m_log->info("cleaning up");

    try {
        // Remove the cached app.
        m_context.storage<objects>("core:cache")->remove("apps", m_manifest.name);    
    } catch(const storage_error_t& e) {
        m_log->warning("unable cleanup the app cache - %s", e.what());
    }

    try {
        // Remove the app from the spool.
        boost::filesystem::remove_all(m_manifest.path);
    } catch(const boost::filesystem::filesystem_error& e) {
        m_log->warning("unable to cleanup the app spool - %s", e.what());
    }

    m_engine.reset();
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

bool app_t::enqueue(const boost::shared_ptr<job_t>& job) {
    return m_engine->enqueue(job);
}
