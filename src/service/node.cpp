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
#include "cocaine/detail/service/node.v2/app.hpp"

#include "cocaine/api/storage.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/traits/dynamic.hpp"

#include "cocaine/tuple.hpp"

#include <blackhole/scoped_attributes.hpp>

#include <boost/spirit/include/karma_char.hpp>
#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_string.hpp>

using namespace cocaine::service;

using namespace blackhole;

namespace ph = std::placeholders;

node_t::node_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args):
    category_type(context, asio, name, args),
    dispatch<io::node_tag>(name),
    m_context(context),
    m_log(context.log(name))
{
    on<io::node::start_app>(std::bind(&node_t::start_app, this, ph::_1, ph::_2));
    on<io::node::pause_app>(std::bind(&node_t::pause_app, this, ph::_1));
    on<io::node::list>     (std::bind(&node_t::list, this));

    const auto runname = args.as_object().at("runlist", "").as_string();

    if(runname.empty()) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "reading '%s' runlist", runname);

    typedef std::map<std::string, std::string> runlist_t;
    runlist_t runlist;

    try {
        const auto storage = api::storage(context, "core");
        // TODO: Perform request to a special service, like "storage->runlist(runname)".
        runlist = storage->get<runlist_t>("runlists", runname);
    } catch(const storage_error_t& err) {
        COCAINE_LOG_WARNING(m_log, "unable to read '%s' runlist: %s", runname, err.what());
    }

    if(runlist.empty()) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "starting %d app(s)", runlist.size());

    std::vector<std::string> errored;

    for(auto it = runlist.begin(); it != runlist.end(); ++it) {
        scoped_attributes_t scope(*m_log, { attribute::make("app", it->first) });

        try {
            start_app(it->first, it->second);
        } catch(const std::exception& e) {
            COCAINE_LOG_WARNING(m_log, "unable to initialize app: %s", e.what());
            errored.push_back(it->first);
        }
    }

    if(!errored.empty()) {
        std::ostringstream stream;
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::string % ", ", errored);

        COCAINE_LOG_WARNING(m_log, "couldn't start %d app(s): %s", errored.size(), stream.str());
    }
}

node_t::~node_t() {}

const basic_dispatch_t&
node_t::prototype() const {
    return *this;
}

void
node_t::start_app(const std::string& name, const std::string& profile) {
    COCAINE_LOG_DEBUG(m_log, "performing `start '%s' app` request", name);

    m_apps.apply([this, &name, &profile](std::map<std::string, std::shared_ptr<v2::app_t>>& apps){
        auto it = apps.find(name);

        if(it != apps.end()) {
            throw cocaine::error_t("app '%s' is already running", name);
        }

        auto app = std::make_shared<v2::app_t>(m_context, name, profile);
        app->start();

        apps.insert({ name, std::move(app) });
    });
}

void
node_t::pause_app(const std::string& name) {
    COCAINE_LOG_DEBUG(m_log, "performing `pause '%s' app` request", name);

    m_apps.apply([&name](std::map<std::string, std::shared_ptr<v2::app_t>>& apps){
        auto it = apps.find(name);

        if(it == apps.end()) {
            throw cocaine::error_t("app '%s' is not running", name);
        }

        apps.erase(it);
    });
}

auto
node_t::list() const -> dynamic_t {
    dynamic_t::array_t result;
    auto builder = std::back_inserter(result);

    m_apps.apply([&builder](const std::map<std::string, std::shared_ptr<v2::app_t>>& apps){
        std::transform(apps.begin(), apps.end(), builder, tuple::nth_element<0>());
    });

    return result;
}
