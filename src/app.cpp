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

#include "cocaine/app.hpp"

#include "cocaine/engine.hpp"
#include "cocaine/drivers.hpp"

using namespace cocaine;
using namespace cocaine::engine;

app_t::app_t(context_t& context, const std::string& name):
	m_engine(new engine_t(context, name))
{
    // Json::Value events(m_app.manifest["tasks"]);

    // if(!events.isNull() && events.size()) {
    //     Json::Value::Members names(events.getMemberNames());

    //     m_app.log->info(
    //         "initializing drivers for %zu %s: %s",
    //         events.size(),
    //         events.size() == 1 ? "events" : "events",
    //         boost::algorithm::join(names, ", ").c_str()
    //     );
    
    //     for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
    //         std::string event(*it);
    //         std::string type(events[event]["type"].asString());
            
    //         if(type == "recurring-timer") {
    //             m_drivers.insert(event, new drivers::recurring_timer_t(*this, event, events[event]));
    //         } else if(type == "drifting-timer") {
    //             m_drivers.insert(event, new drivers::drifting_timer_t(*this, event, events[event]));
    //         } else if(type == "filesystem-monitor") {
    //             m_drivers.insert(event, new drivers::filesystem_monitor_t(*this, event, events[event]));
    //         } else if(type == "zeromq-server") {
    //             m_drivers.insert(event, new drivers::zeromq_server_t(*this, event, events[event]));
    //         } else if(type == "zeromq-sink") {
    //             m_drivers.insert(event, new drivers::zeromq_sink_t(*this, event, events[event]));
    //         } else if(type == "server+lsd" || type == "native-server") {
    //             m_drivers.insert(event, new drivers::native_server_t(*this, event, events[event]));
    //         } else {
    //            throw configuration_error_t("no driver for '" + type + "' is available");
    //         }
    //     }
    // } else {
    //     throw configuration_error_t("no events has been specified");
    // }
}

app_t::~app_t() {
    m_engine.reset();
    // m_drivers.clear();
}

void app_t::start() {
	m_engine->start();
}

void app_t::stop() {
	m_engine->stop();
}

Json::Value app_t::info() const {
    // for(driver_map_t::iterator it = m_drivers.begin();
    //     it != m_drivers.end();
    //     ++it) 
    // {
    //     results["tasks"][it->first] = it->second->info();
    // }

	return m_engine->info();
}

void app_t::enqueue(const boost::shared_ptr<job_t>& job) {
	m_engine->enqueue(job);
}
