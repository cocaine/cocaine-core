/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/service/node.hpp"
#include "cocaine/detail/service/node/app.hpp"

#include "cocaine/api/storage.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/traits/dynamic.hpp"

#include <blackhole/scoped_attributes.hpp>

#include <tuple>

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

node_t::node_t(context_t& context, boost::asio::io_service& asio, const std::string& name, const dynamic_t& args):
    api::service_t(context, asio, name, args),
    dispatch<node_tag>(name),
    m_context(context),
    m_log(context.log(name))
{
    using namespace std::placeholders;

    on<node::start_app>(std::bind(&node_t::on_start_app, this, _1));
    on<node::pause_app>(std::bind(&node_t::on_pause_app, this, _1));
    on<node::list>(std::bind(&node_t::on_list, this));

    std::map<std::string, std::string> runlist;

    const auto runlist_id = args.as_object().at("runlist", "default").as_string();

    // It's here to keep the reference alive.
    const auto storage = api::storage(m_context, "core");

    try {
        COCAINE_LOG_INFO(m_log, "reading the runlist")(
            "runlist", runlist_id
        );

        runlist = storage->get<decltype(runlist)>("runlists", runlist_id);
    } catch(const storage_error_t& e) {
        COCAINE_LOG_WARNING(m_log, "unable to read the runlist: %s", e.what())(
            "runlist", runlist_id
        );
    }

    if(!runlist.empty()) {
        COCAINE_LOG_INFO(m_log, "starting %d app(s)", runlist.size());

        // NOTE: Ignore the return value here, as there's nowhere to return it. It might be nice to
        // parse and log it in case of errors or simply die.
        on_start_app(runlist);
    }
}

node_t::~node_t() {
    auto ptr = m_apps.synchronize();

    if(!ptr->empty()) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "stopping the apps");

    for(auto it = ptr->begin(); it != ptr->end(); ++it) {
        it->second->stop();
    }

    ptr->clear();
}

auto
node_t::prototype() const -> const basic_dispatch_t& {
    return *this;
}

dynamic_t
node_t::on_start_app(const std::map<std::string, std::string>& runlist) {
    dynamic_t::object_t result;

    for(auto it = runlist.begin(); it != runlist.end(); ++it) {
        blackhole::scoped_attributes_t attributes(*m_log, {
            blackhole::attribute::make("app", it->first)
        });

        if(m_apps->count(it->first)) {
            result[it->first] = "the app is already running";
            continue;
        }

        COCAINE_LOG_INFO(m_log, "starting the app");

        auto ptr = m_apps.synchronize();
        auto app = ptr->end();

        try {
            std::tie(app, std::ignore) = ptr->insert({
                it->first,
                std::make_shared<app_t>(m_context, it->first, it->second)
            });
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(m_log, "unable to initialize the app: %s", e.what());
            result[it->first] = std::string(e.what());
            continue;
        }

        try {
            app->second->start();
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(m_log, "unable to start the app: %s", e.what());
            ptr->erase(app);
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

        COCAINE_LOG_INFO(m_log, "stopping the app")(
            "name", *it
        );

        auto ptr = m_apps.synchronize();
        auto app = ptr->find(*it);

        app->second->stop();
        ptr->erase(app);

        result[*it] = "the app has been stopped";
    }

    return result;
}

dynamic_t
node_t::on_list() const {
    dynamic_t::array_t result;

    auto ptr = m_apps.synchronize();

    for(auto it = ptr->begin(); it != ptr->end(); ++it) {
        result.push_back(it->first);
    }

    return result;
}
