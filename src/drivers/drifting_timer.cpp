//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#include "cocaine/dealer/types.hpp"
#include "cocaine/drivers/drifting_timer.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;

drifting_timer_job_t::drifting_timer_job_t(drifting_timer_t& driver, const client::policy_t& policy):
    publication_t(driver, policy)
{ }

void drifting_timer_job_t::react(const events::error_t& event) {
    job_t::react(event);
    static_cast<drifting_timer_t&>(m_driver).rearm();
}

void drifting_timer_job_t::react(const events::choked_t& event) {
    static_cast<drifting_timer_t&>(m_driver).rearm();
}

drifting_timer_t::drifting_timer_t(engine_t& engine, const std::string& method, const Json::Value& args):
    recurring_timer_t(engine, method, args)
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
    client::policy_t policy(false, m_interval, 0.0f);
    boost::shared_ptr<drifting_timer_job_t> job(new drifting_timer_job_t(*this, policy));
    
    m_watcher.stop();
    
    m_engine.enqueue(job);
}
