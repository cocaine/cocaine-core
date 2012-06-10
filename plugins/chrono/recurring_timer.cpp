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

#include "recurring_timer.hpp"

#include "cocaine/engine.hpp"
#include "cocaine/job.hpp"

using namespace cocaine;
using namespace cocaine::engine::drivers;

recurring_timer_t::recurring_timer_t(context_t& context, engine_t& engine, const plugin_config_t& config):
    category_type(context, engine, config),
    m_event(config.args["emit"].asString()),
    m_interval(config.args.get("interval", 0.0f).asInt() / 1000.0f),
    m_watcher(engine.loop())
{
    if(m_interval <= 0.0f) {
        throw configuration_error_t("no interval has been specified");
    }

    m_watcher.set<recurring_timer_t, &recurring_timer_t::event>(this);
    m_watcher.start(m_interval, m_interval);
}

recurring_timer_t::~recurring_timer_t() {
    m_watcher.stop();
}

Json::Value recurring_timer_t::info() const {
    Json::Value result;

    result["type"] = "recurring-timer";
    result["interval"] = m_interval;

    return result;
}

void recurring_timer_t::event(ev::timer&, int) {
    reschedule();
}

void recurring_timer_t::reschedule() {
    engine().enqueue(
        boost::make_shared<job_t>(
            m_event
        )
    );
}
