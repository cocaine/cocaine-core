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

#include "drifting_timer.hpp"

#include "cocaine/engine.hpp"

using namespace cocaine::engine::drivers;

drifting_timer_job_t::drifting_timer_job_t(const std::string& event, drifting_timer_t& driver):
    job_t(event),
    m_driver(driver)
{ }

drifting_timer_job_t::~drifting_timer_job_t() {
    m_driver.rearm();
}

drifting_timer_t::drifting_timer_t(context_t& context, engine_t& engine, const plugin_config_t& config):
    recurring_timer_t(context, engine, config)
{ }

Json::Value drifting_timer_t::info() const {
    Json::Value result(recurring_timer_t::info());

    result["type"] = "drifting-timer";

    return result;
}

void drifting_timer_t::rearm() {
    m_watcher.again();
}

void drifting_timer_t::reschedule() {
    m_watcher.stop();

    engine().enqueue(
    	boost::make_shared<drifting_timer_job_t>(
    		m_event,
    		boost::ref(*this)
    	)
    );
}
