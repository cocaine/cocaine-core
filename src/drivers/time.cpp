/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/drivers/time.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/app.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::driver;

recurring_timer_t::recurring_timer_t(context_t& context, io::reactor_t& reactor, app_t& app, const std::string& name, const Json::Value& args):
    category_type(context, reactor, app, name, args),
    m_log(new logging::log_t(context, cocaine::format("app/%s", name))),
    m_app(app),
    m_event(args.get("emit", name).asString()),
    m_interval(args.get("interval", 0.0f).asInt() / 1000.0f),
    m_watcher(reactor.native())
{
    if(m_interval <= 0.0f) {
        throw cocaine::error_t("no interval has been specified");
    }

    m_watcher.set<recurring_timer_t, &recurring_timer_t::on_event>(this);
    m_watcher.start(m_interval, m_interval);
}

recurring_timer_t::~recurring_timer_t() {
    m_watcher.stop();
}

Json::Value
recurring_timer_t::info() const {
    Json::Value result;

    result["type"] = "recurring-timer";
    result["interval"] = m_interval;

    return result;
}

void
recurring_timer_t::on_event(ev::timer&, int) {
    try {
        m_app.enqueue(api::event_t(m_event), std::make_shared<api::null_stream_t>());
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to enqueue an event - %s", e.what());
    }
}

