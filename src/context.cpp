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
#include "cocaine/context/config.hpp"
#include "cocaine/context/mapper.hpp"
#include "cocaine/context/signal.hpp"
#include "cocaine/detail/essentials.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/format.hpp"
#include "cocaine/idl/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/rpc/actor.hpp"

#include <boost/optional/optional.hpp>

#include <boost/spirit/include/karma_char.hpp>
#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_string.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>
#include <blackhole/wrapper.hpp>

#include <deque>

namespace cocaine {

using namespace cocaine::io;

using blackhole::scope::holder_t;

struct context_t::impl_t {
    impl_t(config_t _config, std::unique_ptr<logging::logger_t> _log) :
        log(std::move(_log)),
        config(_config),
        mapper(config)
    {}
    typedef std::deque<std::pair<std::string, std::unique_ptr<actor_t>>> service_list_t;

    // TODO: There was an idea to use the Repository to enable pluggable sinks and whatever else for
    // for the Blackhole, when all the common stuff is extracted to a separate library.
    std::unique_ptr<logging::logger_t> log;

    // NOTE: This is the first object in the component tree, all the other dynamic components, be it
    // storages or isolates, have to be declared after this one.
    std::unique_ptr<api::repository_t> repository;

    // A pool of execution units - threads responsible for doing all the service invocations.
    std::vector<std::unique_ptr<execution_unit_t>> pool;

    // Services are stored as a vector of pairs to preserve the initialization order. Synchronized,
    // because services are allowed to start and stop other services during their lifetime.
    synchronized<service_list_t> services;

    // Context signalling hub.
    retroactive_signal<io::context_tag> signals;

    const config_t config;

    // Service port mapping and pinning.
    port_mapping_t mapper;

};

void
context_t::impl_deleter_t::operator()(impl_t* ptr) const {
    delete ptr;
}


context_t::context_t(config_t config, std::unique_ptr<logging::logger_t> logger) :
    impl(new impl_t(std::move(config), std::move(logger))) {

    const holder_t scoped(*impl->log, {{"source", "core"}});

    COCAINE_LOG_INFO(impl->log, "initializing the core");

    impl->repository = std::make_unique<api::repository_t>(log("repository"));

    // Load the builtin plugins.
    essentials::initialize(*impl->repository);

    // Load the rest of plugins.
    impl->repository->load(impl->config.path.plugins);

    // Spin up all the configured services, launch execution units.
    COCAINE_LOG_INFO(impl->log, "starting {:d} execution unit(s)", config.network.pool);

    while (impl->pool.size() != config.network.pool) {
        impl->pool.emplace_back(std::make_unique<execution_unit_t>(*this));
    }

    COCAINE_LOG_INFO(impl->log, "starting {:d} service(s)", config.services.size());

    std::vector<std::string> errored;

    for (auto it = config.services.begin(); it != config.services.end(); ++it) {
        const holder_t scoped(*impl->log, {{"service", it->first}});

        const auto asio = std::make_shared<asio::io_service>();

        COCAINE_LOG_DEBUG(impl->log, "starting service");

        try {
            insert(it->first, std::make_unique<actor_t>(*this, asio, repository().get<api::service_t>(
            it->second.type,
            *this,
            *asio,
            it->first,
            it->second.args
            )));
        } catch (const std::system_error& e) {
            COCAINE_LOG_ERROR(impl->log, "unable to initialize service: {}", error::to_string(e));
            errored.push_back(it->first);
        } catch (const std::exception& e) {
            COCAINE_LOG_ERROR(impl->log, "unable to initialize service: {}", e.what());
            errored.push_back(it->first);
        }
    }

    if (!errored.empty()) {
        COCAINE_LOG_ERROR(impl->log, "emergency core shutdown");

        // Signal and stop all the services, shut down execution units.
        terminate();

        std::ostringstream stream;
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::string % ", ", errored);

        throw cocaine::error_t("couldn't start core because of %d service(s): %s",
                               errored.size(), stream.str()
        );
    } else {
        impl->signals.invoke<io::context::prepared>();
    }
}

context_t::~context_t() {
    const holder_t scoped(*impl->log, {{"source", "core"}});

    // Signal and stop all the services, shut down execution units.
    terminate();
}

//void
//context_t::listen(const std::shared_ptr<dispatch<io::context_tag>>& slot, asio::io_service& asio) {
//    impl->signals.listen(slot, asio);
//}
//
//template<class Event, class... Args>
//void
//context_t::invoke(const Args&... args) {
//    impl->signals.invoke<Event>(args...);
//}
//
//template
//void
//context_t::invoke<io::context::os_signal, int, siginfo_t>(const int&, const siginfo_t&);
//
//template
//void
//context_t::invoke<io::context::prepared>();
//
//template
//void
//context_t::invoke<io::context::shutdown>();
//
//template
//void
//context_t::invoke<io::context::service::exposed,
//                  std::string,
//                  std::tuple<std::vector<asio::ip::tcp::endpoint>, unsigned int, io::graph_root_t>>(
//    const std::string&,
//    const std::tuple<std::vector<asio::ip::tcp::endpoint>, unsigned int, io::graph_root_t>&);
//
//template
//void
//context_t::invoke<io::context::service::removed,
//                  std::string,
//                  std::tuple<std::vector<asio::ip::tcp::endpoint>, unsigned int, io::graph_root_t>>(
//    const std::string&,
//    const std::tuple<std::vector<asio::ip::tcp::endpoint>, unsigned int, io::graph_root_t>&);

std::unique_ptr<logging::logger_t>
context_t::log(const std::string& source) {
    return log(source, {});
}

std::unique_ptr<logging::logger_t>
context_t::log(const std::string& source, blackhole::attributes_t attributes) {
    attributes.push_back({"source", {source}});

    // TODO: Make it possible to use in-place operator+= to fill in more attributes?
    return std::make_unique<blackhole::wrapper_t>(*impl->log, std::move(attributes));
}

const api::repository_t&
context_t::repository() const {
    return *impl->repository;
}

retroactive_signal<io::context_tag>&
context_t::signal_hub() {
    return impl->signals;
}

const config_t&
context_t::config() const {
    return impl->config;
}

port_mapping_t&
context_t::mapper(){
    return impl->mapper;
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
    const holder_t scoped(*impl->log, {{"source", "core"}});

    const actor_t& actor = *service;

    impl->services.apply([&](impl_t::service_list_t& list) {
        if (std::count_if(list.begin(), list.end(), match{name})) {
            throw cocaine::error_t("service '%s' already exists", name);
        }

        service->run();

        COCAINE_LOG_DEBUG(impl->log, "service has been started", {
            { "service", { name }}
        });

        list.emplace_back(name, std::move(service));
    });

    // Fire off the signal to alert concerned subscribers about the service removal event.
    impl->signals.invoke<context::service::exposed>(actor.prototype().name(), std::forward_as_tuple(
        actor.endpoints(),
        actor.prototype().version(),
        actor.prototype().root()
    ));
}

std::unique_ptr<actor_t>
context_t::remove(const std::string& name) {
    const holder_t scoped(*impl->log, {{"source", "core"}});

    std::unique_ptr<actor_t> service;

    impl->services.apply([&](impl_t::service_list_t& list) {
        auto it = std::find_if(list.begin(), list.end(), match{name});

        if (it == list.end()) {
            throw cocaine::error_t("service '%s' doesn't exist", name);
        }

        service = std::move(it->second);
        list.erase(it);
    });

    service->terminate();

    COCAINE_LOG_DEBUG(impl->log, "service has been stopped", {
        { "service", name }
    });

    // Service is already terminated, so there's no reason to try to get its endpoints.
    std::vector<asio::ip::tcp::endpoint> nothing;

    // Fire off the signal to alert concerned subscribers about the service termination event.
    impl->signals.invoke<context::service::removed>(service->prototype().name(), std::forward_as_tuple(
        nothing,
        service->prototype().version(),
        service->prototype().root()
    ));

    return service;
}

boost::optional<const actor_t&>
context_t::locate(const std::string& name) const {
    auto ptr = impl->services.synchronize();
    auto it = std::find_if(ptr->begin(), ptr->end(), match{name});

    if (it == ptr->end()) {
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
    return **std::min_element(impl->pool.begin(), impl->pool.end(), utilization_t());
}

void
context_t::bootstrap() {

}

void
context_t::terminate() {
    COCAINE_LOG_INFO(impl->log, "stopping {:d} service(s)", impl->services->size());

    // Fire off to alert concerned subscribers about the shutdown. This signal happens before all
    // the outstanding connections are closed, so services have a chance to send their last wishes.
    impl->signals.invoke<context::shutdown>();

    // Stop the service from accepting new clients or doing any processing. Pop them from the active
    // service list into this temporary storage, and then destroy them all at once. This is needed
    // because sessions in the execution units might still have references to the services, and their
    // lives have to be extended until those sessions are active.
    std::vector<std::unique_ptr<actor_t>> actors;

    for (auto it = impl->config.services.rbegin(); it != impl->config.services.rend(); ++it) {
        try {
            actors.push_back(remove(it->first));
        } catch (...) {
            // A service might be absent because it has failed to start during the bootstrap.
            continue;
        }
    }

    // There should be no outstanding services left. All the extra services spawned by others, like
    // app invocation services from the node service, should be dead by now.
    BOOST_ASSERT(impl->services->empty());

    COCAINE_LOG_INFO(impl->log, "stopping {:d} execution unit(s)", impl->pool.size());

    impl->pool.clear();

    // Destroy the service objects.
    actors.clear();

    COCAINE_LOG_INFO(impl->log, "core has been terminated");
}

} //  namespace cocaine
