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

#include "cocaine/services/node.hpp"

#include "cocaine/app.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/api/storage.hpp"

#include "cocaine/traits/json.hpp"

#include <boost/tuple/tuple.hpp>

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::logging;
using namespace cocaine::service;

namespace {
    typedef std::map<
        std::string,
        std::string
    > runlist_t;
}

node_t::node_t(context_t& context, 
               const std::string& name,
               const Json::Value& args):
    reactor_t(context, name, args),
    m_context(context),
    m_log(new log_t(context, name)),
    m_announces(context, ZMQ_PUB),
    m_announce_timer(loop()),
    m_birthstamp(loop().now())
{
    on<io::node::start_app>(boost::bind(&node_t::on_start_app, this, _1));
    on<io::node::pause_app>(boost::bind(&node_t::on_pause_app, this, _1));
    on<io::node::info>(boost::bind(&node_t::on_info, this));
    
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);

    COCAINE_LOG_INFO(m_log, "using libev version %d.%d", ev_version_major(), ev_version_minor());
    COCAINE_LOG_INFO(m_log, "using libmsgpack version %s", msgpack_version());
    COCAINE_LOG_INFO(m_log, "using libzmq version %d.%d.%d", major, minor, patch);

    // Configuration

    const ev::tstamp interval = args.get("announce-interval", 5.0f).asDouble();
    const std::string runlist_id = args.get("runlist", "default").asString();
    
    // Autodiscovery

    for(Json::Value::const_iterator it = args["announce"].begin();
        it != args["announce"].end();
        ++it)
    {
        std::string endpoint((*it).asString());

        COCAINE_LOG_INFO(m_log, "announcing on %s", endpoint);

        try {
            m_announces.bind(endpoint);
        } catch(const zmq::error_t& e) {
            throw configuration_error_t("invalid announce endpoint - %s", e.what());
        }
    }

    m_announce_timer.set<node_t, &node_t::on_announce>(this);
    m_announce_timer.start(0.0f, interval);

    // Runlist

    COCAINE_LOG_INFO(m_log, "reading the '%s' runlist", runlist_id);
    
    const runlist_t runlist = api::storage(m_context, "core")->get<
        std::map<std::string, std::string>
    >("runlists", runlist_id);
    
    on_start_app(runlist);
}

node_t::~node_t() {
    if(!m_apps.empty()) {
        COCAINE_LOG_INFO(m_log, "stopping the apps");
        m_apps.clear();
    }
}

void
node_t::on_announce(ev::timer&, int) {
    COCAINE_LOG_DEBUG(m_log, "announcing the node");

    m_announces.send_multipart(
        protect(m_context.config.network.hostname),
        on_info()
    );
}

Json::Value
node_t::on_start_app(runlist_t runlist) {
    Json::Value result;
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
            boost::tie(app, boost::tuples::ignore) = m_apps.emplace(
                it->first,
                boost::make_shared<app_t>(
                    m_context,
                    it->first,
                    it->second
                )
            );
        } catch(const storage_error_t& e) {
            result[it->first] = "the app was not found in the storage";
            continue;
        }

        try {
            app->second->start();
        } catch(const std::exception& e) {
            m_apps.erase(app);
            result[it->first] = e.what();
            continue;
        }

        result[it->first] = "the app has been started";
    }

    return result;
}

Json::Value
node_t::on_pause_app(std::vector<std::string> applist) {
    Json::Value result;
    
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
    result["uptime"] = loop().now() - m_birthstamp;

    return result;
}
