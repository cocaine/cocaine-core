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

#include "filesystem_monitor.hpp"

#include "cocaine/engine.hpp"
#include "cocaine/job.hpp"

using namespace cocaine;
using namespace cocaine::engine::drivers;

filesystem_monitor_t::filesystem_monitor_t(context_t& context, engine_t& engine, const plugin_config_t& config):
    category_type(context, engine, config),
    m_path(config.args.get("path", "").asString()),
    m_watcher(engine.loop())
{
    if(m_path.empty()) {
        throw configuration_error_t("no path has been specified");
    }
    
    m_watcher.set<filesystem_monitor_t, &filesystem_monitor_t::event>(this);
    m_watcher.start(m_path.c_str());
}

filesystem_monitor_t::~filesystem_monitor_t() {
    m_watcher.stop();
}

Json::Value filesystem_monitor_t::info() const {
    Json::Value result;

    result["type"] = "filesystem-monitor";
    result["path"] = m_path;

    return result;
}

void filesystem_monitor_t::event(ev::stat&, int) {
    engine().enqueue(
        boost::make_shared<job_t>(
            m_event
        )
    );
}

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<filesystem_monitor_t>("filesystem-monitor");
    }
}
