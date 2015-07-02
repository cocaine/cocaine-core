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
#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/tuple.hpp"
#include "cocaine/traits/vector.hpp"

#include "cocaine/tuple.hpp"

#include <blackhole/scoped_attributes.hpp>

#include <boost/spirit/include/karma_char.hpp>
#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_string.hpp>

using namespace cocaine;
using namespace cocaine::service;

namespace ph = std::placeholders;

namespace {

// Node Service errors

struct node_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.service.node";
    }

    virtual
    std::string
    message(int code) const {
        switch (code) {
        case error::node_errors::deadline_error:
            return "invocation deadline has passed";
        case error::node_errors::resource_error:
            return "no resources available to complete invocation";
        case error::node_errors::timeout_error:
            return "invocation has timed out";
        default:
            break;
        }

        return cocaine::format("unknown node error %d", code);
    }
};

} // namespace

namespace cocaine { namespace error {

auto
node_category() -> const std::error_category& {
    static node_category_t instance;
    return instance;
}

auto
make_error_code(node_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), node_category());
}

}} // namespace cocaine::error

node_t::node_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args):
    category_type(context, asio, name, args),
    dispatch<io::node_tag>(name),
    context(context),
    log(context.log(name))
{
    on<io::node::start_app>(std::bind(&node_t::start_app, this, ph::_1, ph::_2));
    on<io::node::pause_app>(std::bind(&node_t::pause_app, this, ph::_1));
    on<io::node::list>     (std::bind(&node_t::list, this));
    on<io::node::info>     (std::bind(&node_t::info, this, ph::_1));

    const auto runname = args.as_object().at("runlist", "").as_string();

    if(runname.empty()) {
        return;
    }

    COCAINE_LOG_INFO(log, "reading '%s' runlist", runname);

    typedef std::map<std::string, std::string> runlist_t;
    runlist_t runlist;

    try {
        const auto storage = api::storage(context, "core");
        // TODO: Perform request to a special service, like "storage->runlist(runname)".
        runlist = storage->get<runlist_t>("runlists", runname);
    } catch(const std::system_error& err) {
        COCAINE_LOG_WARNING(log, "unable to read '%s' runlist: %s", runname, err.what());
    }

    if(runlist.empty()) {
        return;
    }

    COCAINE_LOG_INFO(log, "starting %d app(s)", runlist.size());

    std::vector<std::string> errored;

    for(auto it = runlist.begin(); it != runlist.end(); ++it) {
        blackhole::scoped_attributes_t scope(*log, {{ "app", it->first }});

        try {
            start_app(it->first, it->second);
        } catch(const std::exception& e) {
            COCAINE_LOG_WARNING(log, "unable to initialize app: %s", e.what());
            errored.push_back(it->first);
        }
    }

    if(!errored.empty()) {
        std::ostringstream stream;
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::string % ", ", errored);

        COCAINE_LOG_WARNING(log, "couldn't start %d app(s): %s", errored.size(), stream.str());
    }

    // Context signal/slot.
    signal = std::make_shared<dispatch<io::context_tag>>(name);
    signal->on<io::context::shutdown>(std::bind(&node_t::on_context_shutdown, this));
    context.listen(signal, asio);
}

node_t::~node_t() {}

auto
node_t::prototype() const -> const io::basic_dispatch_t&{
    return *this;
}

void
node_t::on_context_shutdown() {
    COCAINE_LOG_DEBUG(log, "shutting down apps");

    apps->clear();

    signal = nullptr;
}

deferred<void>
node_t::start_app(const std::string& name, const std::string& profile) {
    COCAINE_LOG_DEBUG(log, "processing `start_app` request, app: '%s'", name);

    cocaine::deferred<void> deferred;

    apps.apply([&](std::map<std::string, std::shared_ptr<node::app_t>>& apps) {
        auto it = apps.find(name);

        if(it != apps.end()) {
            // TODO: Handling app state by parsing strings seems to be not the best idea.
            const auto info = it->second->info();
            const auto state = info.as_object()["state"].as_string();

            if (state == "stopped") {
                const auto reason = info.as_object()["cause"].as_string();
                throw cocaine::error_t("app '%s' is stopped, reason: %s", name, reason);
            } else if (state == "spooling") {
                throw cocaine::error_t("app '%s' is spooling");
            }

            throw cocaine::error_t("app '%s' is already running", name);
        }

        apps.insert({ name, std::make_shared<node::app_t>(context, name, profile, deferred) });
    });

    return deferred;
}

void
node_t::pause_app(const std::string& name) {
    COCAINE_LOG_DEBUG(log, "processing `pause_app` request, app: '%s' ", name);

    apps.apply([&](std::map<std::string, std::shared_ptr<node::app_t>>& apps) {
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

    apps.apply([&](const std::map<std::string, std::shared_ptr<node::app_t>>& apps) {
        std::transform(apps.begin(), apps.end(), builder, tuple::nth_element<0>());
    });

    return result;
}

dynamic_t
node_t::info(const std::string& name) const {
    auto app = apps.apply([&](const std::map<std::string, std::shared_ptr<node::app_t>>& apps) -> std::shared_ptr<node::app_t> {
        auto it = apps.find(name);

        if(it != apps.end()) {
            return it->second;
        }

        return nullptr;
    });

    if (!app) {
        throw cocaine::error_t("app '%s' is not running", name);
    }

    return app->info();
}
