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

#include "cocaine/context.hpp"

#include "cocaine/api/service.hpp"

#include "cocaine/detail/engine.hpp"
#include "cocaine/detail/essentials.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/rpc/actor.hpp"

#include <blackhole/scoped_attributes.hpp>

#include <boost/spirit/include/karma_char.hpp>
#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_string.hpp>

using namespace cocaine;
using namespace cocaine::io;

using namespace blackhole;

context_t::context_t(config_t config_, std::unique_ptr<logging::log_t> log_):
    config(config_),
    mapper(config_)
{
    m_log = std::move(log_);

    scoped_attributes_t guard(*m_log, attribute::set_t({logging::keyword::source() = "core"}));

    COCAINE_LOG_INFO(m_log, "initializing the core");

    m_repository = std::make_unique<api::repository_t>(log("repository"));

    // Load the builtin plugins.
    essentials::initialize(*m_repository);

    // Load the rest of plugins.
    m_repository->load(config.path.plugins);

    // Spin up all the configured services, launch execution units.
    bootstrap();
}

context_t::~context_t() {
    scoped_attributes_t guard(*m_log, attribute::set_t({logging::keyword::source() = "core"}));

    // Signal and stop all the services, shut down execution units.
    terminate();
}

std::unique_ptr<logging::log_t>
context_t::log(const std::string& source, attribute::set_t attributes) {
    attributes.emplace_back(logging::keyword::source() = source);

    // TODO: Make it possible to use in-place operator+= to fill in more attributes?
    return std::make_unique<logging::log_t>(*m_log, std::move(attributes));
}

namespace {

struct match {
    template<class T>
    bool
    operator()(const T& service) const {
        return name == service.first;
    }

    const std::string& name;
};

} // namespace

void
context_t::insert(const std::string& name, std::unique_ptr<actor_t> service) {
    scoped_attributes_t guard(*m_log, attribute::set_t({logging::keyword::source() = "core"}));

    const actor_t& actor = *service;

    m_services.apply([&](service_list_t& list) {
        if(std::count_if(list.begin(), list.end(), match{name})) {
            throw cocaine::error_t("service '%s' already exists", name);
        }

        service->run();

        COCAINE_LOG_DEBUG(m_log, "service has been started")(
            "service", name
        );

        list.emplace_back(name, std::move(service));
    });

    // Fire off the signal to alert concerned subscribers about the service removal event.
    m_signals.invoke<context::service::exposed>(actor.prototype().name(), std::forward_as_tuple(
        actor.endpoints(),
        actor.prototype().version(),
        actor.prototype().root()
    ));
}

std::unique_ptr<actor_t>
context_t::remove(const std::string& name) {
    scoped_attributes_t guard(*m_log, attribute::set_t({logging::keyword::source() = "core"}));

    std::unique_ptr<actor_t> service;

    m_services.apply([&](service_list_t& list) {
        auto it = std::find_if(list.begin(), list.end(), match{name});

        if(it == list.end()) {
            throw cocaine::error_t("service '%s' doesn't exist", name);
        }

        service = std::move(it->second); list.erase(it);
    });

    service->terminate();

    COCAINE_LOG_DEBUG(m_log, "service has been stopped")(
        "service", name
    );

    // Service is already terminated, so there's no reason to try to get its endpoints.
    std::vector<asio::ip::tcp::endpoint> nothing;

    // Fire off the signal to alert concerned subscribers about the service termination event.
    m_signals.invoke<context::service::removed>(service->prototype().name(), std::forward_as_tuple(
        nothing,
        service->prototype().version(),
        service->prototype().root()
    ));

    return service;
}

boost::optional<const actor_t&>
context_t::locate(const std::string& name) const {
    auto ptr = m_services.synchronize();
    auto it  = std::find_if(ptr->begin(), ptr->end(), match{name});

    if(it == ptr->end()) {
        return boost::none;
    }

    return boost::optional<const actor_t&>(it->second->is_active(), *it->second);
}

namespace {

struct utilization_t {
    typedef std::unique_ptr<execution_unit_t> value_type;

    bool
    operator()(const value_type& lhs, const value_type& rhs) const {
        return lhs->utilization() < rhs->utilization();
    }
};

} // namespace

execution_unit_t&
context_t::engine() {
    return **std::min_element(m_pool.begin(), m_pool.end(), utilization_t());
}

void
context_t::bootstrap() {
    COCAINE_LOG_INFO(m_log, "starting %d execution unit(s)", config.network.pool);

    while(m_pool.size() != config.network.pool) {
        m_pool.emplace_back(std::make_unique<execution_unit_t>(*this));
    }

    COCAINE_LOG_INFO(m_log, "starting %d service(s)", config.services.size());

    std::vector<std::string> errored;

    for(auto it = config.services.begin(); it != config.services.end(); ++it) {
        scoped_attributes_t attributes(*m_log, {attribute::make("service", it->first)});

        const auto asio = std::make_shared<asio::io_service>();

        COCAINE_LOG_DEBUG(m_log, "starting service");

        try {
            insert(it->first, std::make_unique<actor_t>(*this, asio, get<api::service_t>(
                it->second.type,
               *this,
               *asio,
                it->first,
                it->second.args
            )));
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to initialize service: [%d] %s - %s", e.code().value(),
                e.code().message(), e.what());
            errored.push_back(it->first);
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(m_log, "unable to initialize service: %s", e.what());
            errored.push_back(it->first);
        }
    }

    if(!errored.empty()) {
        COCAINE_LOG_ERROR(m_log, "emergency core shutdown");

        // Signal and stop all the services, shut down execution units.
        terminate();

        std::ostringstream stream;
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::string % ", ", errored);

        throw cocaine::error_t("couldn't start core because of %d service(s): %s",
            errored.size(), stream.str()
        );
    } else {
        m_signals.invoke<io::context::prepared>();
    }
}

void
context_t::terminate() {
    COCAINE_LOG_INFO(m_log, "stopping %d service(s)", m_services->size());

    // Fire off to alert concerned subscribers about the shutdown. This signal happens before all
    // the outstanding connections are closed, so services have a chance to send their last wishes.
    m_signals.invoke<context::shutdown>();

    // Stop the service from accepting new clients or doing any processing. Pop them from the active
    // service list into this temporary storage, and then destroy them all at once. This is needed
    // because sessions in the execution units might still have references to the services, and their
    // lives have to be extended until those sessions are active.
    std::vector<std::unique_ptr<actor_t>> actors;

    for(auto it = config.services.rbegin(); it != config.services.rend(); ++it) {
        try {
            actors.push_back(remove(it->first));
        } catch(...) {
            // A service might be absent because it has failed to start during the bootstrap.
            continue;
        }
    }

    // There should be no outstanding services left. All the extra services spawned by others, like
    // app invocation services from the node service, should be dead by now.
    BOOST_ASSERT(m_services->empty());

    COCAINE_LOG_INFO(m_log, "stopping %d execution unit(s)", m_pool.size());

    m_pool.clear();

    // Destroy the service objects.
    actors.clear();

    COCAINE_LOG_INFO(m_log, "core has been terminated");
}
