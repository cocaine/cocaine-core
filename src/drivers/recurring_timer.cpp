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
#include "cocaine/drivers/recurring_timer.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;

recurring_timer_t::recurring_timer_t(engine_t& engine, const std::string& method, const Json::Value& args):
    driver_t(engine, method),
    m_interval(args.get("interval", 0.0f).asInt() / 1000.0f)
{
    if(m_interval <= 0.0f) {
        throw std::runtime_error("no interval has been specified for '" + m_method + "' task");
    }

    m_watcher.set<recurring_timer_t, &recurring_timer_t::event>(this);
    m_watcher.start(m_interval, m_interval);
}

recurring_timer_t::~recurring_timer_t() {
    m_watcher.stop();
}

Json::Value recurring_timer_t::info() const {
    Json::Value result(Json::objectValue);

    result["statistics"] = stats();
    result["type"] = "recurring-timer";
    result["interval"] = m_interval;

    return result;
}

void recurring_timer_t::event(ev::timer&, int) {
    reschedule();
}

void recurring_timer_t::reschedule() {
    boost::shared_ptr<publication_t> job(new publication_t(*this, client::policy_t()));
    m_engine.enqueue(job);
}
