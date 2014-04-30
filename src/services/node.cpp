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

#include "cocaine/detail/services/node.hpp"
#include "cocaine/detail/services/node/app.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/traits/dynamic.hpp"

#include <tuple>

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

using namespace std::placeholders;

namespace {

typedef std::map<std::string, std::string> runlist_t;

} // namespace

node_t::node_t(context_t& context, reactor_t& reactor, const std::string& name, const dynamic_t& args):
    api::service_t(context, reactor, name, args),
    implements<io::node_tag>(name),
    m_context(context),
    m_log(new logging::log_t(context, name))
{
    on<io::node::start_app>(std::bind(&node_t::on_start_app, this, _1));
    on<io::node::pause_app>(std::bind(&node_t::on_pause_app, this, _1));
    on<io::node::list>(std::bind(&node_t::on_list, this));

    const auto runlist_id = args.as_object().at("runlist", "default").as_string();

    // It's here to keep the reference alive.
    const auto storage = api::storage(m_context, "core");

    runlist_t runlist;

    COCAINE_LOG_INFO(m_log, "reading the '%s' runlist", runlist_id);

    try {
        runlist = storage->get<runlist_t>("runlists", runlist_id);
    } catch(const storage_error_t& e) {
        COCAINE_LOG_WARNING(m_log, "unable to read the '%s' runlist - %s", runlist_id, e.what());
    }

    if(!runlist.empty()) {
        COCAINE_LOG_INFO(m_log, "starting %d %s", runlist.size(), runlist.size() == 1 ? "app" : "apps");

        // NOTE: Ignore the return value here, as there's nowhere to return it. It might be nice to
        // parse and log it in case of errors or simply die.
        on_start_app(runlist);
    }
}

node_t::~node_t() {
    auto& unlocked = m_apps.value();

    if(unlocked.empty()) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "stopping the apps");

    for(auto it = unlocked.begin(); it != unlocked.end(); ++it) {
        it->second->stop();
    }

    unlocked.clear();
}

auto
node_t::prototype() -> dispatch_t& {
    return *this;
}

dynamic_t
node_t::on_start_app(const runlist_t& runlist) {
    dynamic_t::object_t result;

    for(auto it = runlist.begin(); it != runlist.end(); ++it) {
        if(m_apps->count(it->first)) {
            result[it->first] = "the app is already running";
            continue;
        }

        COCAINE_LOG_INFO(m_log, "starting the '%s' app", it->first);

        auto locked = m_apps.synchronize();
        auto app    = locked->end();

        try {
            std::tie(app, std::ignore) = locked->insert({
                it->first,
                std::make_shared<app_t>(m_context, it->first, it->second)
            });
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(m_log, "unable to initialize the '%s' app - %s", it->first, e.what());
            result[it->first] = std::string(e.what());
            continue;
        }

        try {
            app->second->start();
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(m_log, "unable to start the '%s' app - %s", it->first, e.what());
            locked->erase(app);
            result[it->first] = std::string(e.what());
            continue;
        }

        result[it->first] = "the app has been started";
    }

    return result;
}

dynamic_t
node_t::on_pause_app(const std::vector<std::string>& applist) {
    dynamic_t::object_t result;

    for(auto it = applist.begin(); it != applist.end(); ++it) {
        if(!m_apps->count(*it)) {
            result[*it] = "the app is not running";
            continue;
        }

        COCAINE_LOG_INFO(m_log, "stopping the '%s' app", *it);

        auto locked = m_apps.synchronize();
        auto app    = locked->find(*it);

        app->second->stop();
        locked->erase(app);

        result[*it] = "the app has been stopped";
    }

    return result;
}

dynamic_t
node_t::on_list() const {
    dynamic_t::array_t result;

    auto locked = m_apps.synchronize();

    for(auto it = locked->begin(); it != locked->end(); ++it) {
        result.push_back(it->first);
    }

    return result;
}
