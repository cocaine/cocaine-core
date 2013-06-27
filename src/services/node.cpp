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

#include "cocaine/api/storage.hpp"

#include "cocaine/app.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/traits/json.hpp"

#include <tuple>

using namespace cocaine::service;

using namespace std::placeholders;

namespace {

typedef std::map<std::string, std::string> runlist_t;

}

node_t::node_t(context_t& context, io::reactor_t& reactor, const std::string& name, const Json::Value& args):
    category_type(context, reactor, name, args),
    m_context(context),
    m_log(new logging::log_t(context, name)),
#if defined(__clang__) || defined(HAVE_GCC47)
    m_birthstamp(std::chrono::steady_clock::now())
#else
    m_birthstamp(std::chrono::monotonic_clock::now())
#endif
{
    on<io::node::start_app>("start_app", std::bind(&node_t::on_start_app, this, _1));
    on<io::node::pause_app>("pause_app", std::bind(&node_t::on_pause_app, this, _1));
    on<io::node::info>("info", std::bind(&node_t::on_info, this));

    // Configuration

    const std::string runlist_id = args.get("runlist", "default").asString();

    // Runlist

    runlist_t runlist;

    COCAINE_LOG_INFO(m_log, "reading the '%s' runlist", runlist_id);

    // It's here to keep the reference alive.
    auto storage = api::storage(m_context, "core");

    try {
        runlist = storage->get<runlist_t>("runlists", runlist_id);
    } catch(const storage_error_t& e) {
        COCAINE_LOG_WARNING(
            m_log,
            "unable to read the '%s' runlist - %s",
            runlist_id,
            e.what()
        );
    }

    if(!runlist.empty()) {
        COCAINE_LOG_INFO(
            m_log,
            "starting %d %s",
            runlist.size(),
            runlist.size() == 1 ? "app" : "apps"
        );

        // NOTE: Ignore the return value here, as there's nowhere to return it.
        // It might be nice to parse and log it in case of errors or simply die.
        on_start_app(runlist);
    }
}

node_t::~node_t() {
    if(!m_apps.empty()) {
        COCAINE_LOG_INFO(m_log, "stopping the apps");

        for(auto it = m_apps.begin(); it != m_apps.end(); ++it) {
            it->second->stop();
        }

        m_apps.clear();
    }
}

Json::Value
node_t::on_start_app(const runlist_t& runlist) {
    Json::Value result(Json::objectValue);
    app_map_t::iterator app;

    for(runlist_t::const_iterator it = runlist.begin();
        it != runlist.end();
        ++it)
    {
        if(m_apps.find(it->first) != m_apps.end()) {
            result[it->first] = "the app is already running";
            continue;
        }

        COCAINE_LOG_INFO(m_log, "starting the '%s' app", it->first);

        try {
            std::tie(app, std::ignore) = m_apps.insert(std::make_pair(
                it->first,
                std::make_shared<app_t>(
                    m_context,
                    it->first,
                    it->second
                )
            ));
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(
                m_log,
                "unable to initialize the '%s' app - %s",
                it->first,
                e.what()
            );

            result[it->first] = e.what();

            continue;
        }

        try {
            app->second->start();
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(
                m_log,
                "unable to start the '%s' app - %s",
                it->first,
                e.what()
            );

            m_apps.erase(app);

            result[it->first] = e.what();

            continue;
        }

        result[it->first] = "the app has been started";
    }

    return result;
}

Json::Value
node_t::on_pause_app(const std::vector<std::string>& applist) {
    Json::Value result(Json::objectValue);

    for(std::vector<std::string>::const_iterator it = applist.begin();
        it != applist.end();
        ++it)
    {
        app_map_t::iterator app(m_apps.find(*it));

        if(app == m_apps.end()) {
            result[*it] = "the app is not running";
            continue;
        }

        COCAINE_LOG_INFO(m_log, "stopping the '%s' app", *it);

        app->second->stop();
        m_apps.erase(app);

        result[*it] = "the app has been stopped";
    }

    return result;
}

Json::Value
node_t::on_info() const {
    Json::Value result(Json::objectValue);

    for(app_map_t::const_iterator it = m_apps.begin();
        it != m_apps.end();
        ++it)
    {
        result["apps"][it->first] = it->second->info();
    }

    result["identity"] = m_context.config.network.hostname;

    using namespace std::chrono;

    auto uptime = duration_cast<seconds>(
#if defined(__clang__) || defined(HAVE_GCC47)
        steady_clock::now() - m_birthstamp
#else
        monotonic_clock::now() - m_birthstamp
#endif
    );

    result["uptime"] = static_cast<Json::LargestUInt>(uptime.count());

    return result;
}
