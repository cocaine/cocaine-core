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
