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
