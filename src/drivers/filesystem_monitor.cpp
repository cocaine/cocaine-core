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

#include "cocaine/drivers/filesystem_monitor.hpp"

#include "cocaine/engine.hpp"
#include "cocaine/job.hpp"

using namespace cocaine::engine::drivers;

filesystem_monitor_t::filesystem_monitor_t(engine_t& engine, const std::string& method, const Json::Value& args):
    driver_t(engine, method, args),
    m_path(args.get("path", "").asString())
{
    if(m_path.empty()) {
        throw std::runtime_error("no path has been specified");
    }
    
    m_watcher.set<filesystem_monitor_t, &filesystem_monitor_t::event>(this);
    m_watcher.start(m_path.c_str());
}

filesystem_monitor_t::~filesystem_monitor_t() {
    m_watcher.stop();
}

Json::Value filesystem_monitor_t::info() {
    Json::Value result(driver_t::info());

    result["type"] = "filesystem-monitor";
    result["path"] = m_path;

    return result;
}

void filesystem_monitor_t::event(ev::stat&, int) {
    m_engine.enqueue(
        boost::make_shared<job_t>(
            boost::ref(*this)
        )
    );
}

