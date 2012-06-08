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

#include "cocaine/app.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::engine;

app_t::app_t(context_t& context, const std::string& name):
    m_log(context.log("app/" + name)),
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
        m_drivers.insert(
            *it,
            context.get<drivers::driver_t>(
                drivers[*it]["type"].asString(),
                category_traits<drivers::driver_t>::args_type(
                    *m_engine,
                    drivers[*it]
                )
            )
        );
    }
}

app_t::~app_t() {
    m_drivers.clear();
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

void app_t::enqueue(const boost::shared_ptr<job_t>& job) {
    m_engine->enqueue(job);
}
