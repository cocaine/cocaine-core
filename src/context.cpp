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
#include "context/distributor.hpp"

#include "cocaine/api/cluster.hpp"
#include "cocaine/api/service.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/context/filter.hpp"
#include "cocaine/context/mapper.hpp"
#include "cocaine/context/quote.hpp"
#include "cocaine/context/signal.hpp"
#include "cocaine/detail/essentials.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/format.hpp"
#include "cocaine/format/exception.hpp"
#include "cocaine/idl/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/rpc/actor.hpp"
#include "cocaine/repository/service.hpp"
#include "cocaine/trace/logger.hpp"
#include "cocaine/format/vector.hpp"
#include "cocaine/format/tuple.hpp"

#include <boost/optional/optional.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>
#include <blackhole/wrapper.hpp>

#include <metrics/registry.hpp>

#include <deque>
#include <exception>

#include "chamber.hpp"

namespace cocaine {

namespace io {

using asio::io_service;

} // namespace io

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

using blackhole::scope::holder_t;

class context_impl_t : public context_t {
    using service_list_t = std::deque<std::pair<std::string, std::unique_ptr<tcp_actor_t>>>;
    using engine_pool_t = std::vector<std::unique_ptr<execution_unit_t>>;
    // TODO: There was an idea to use the Repository to enable pluggable sinks and whatever else for
    // for the Blackhole, when all the common stuff is extracted to a separate library.
    std::unique_ptr<logging::trace_wrapper_t> m_log;

    // NOTE: This is the first object in the component tree, all the other dynamic components, be it
    // storages or isolates, have to be declared after this one.
    std::unique_ptr<api::repository_t> m_repository;

    // An acceptor thread.
    std::unique_ptr<io::chamber_t> m_acceptor_thread;

    // A pool of execution units - threads responsible for doing all the service invocations.
    engine_pool_t m_pool;

    // Services are stored as a vector of pairs to preserve the initialization order. Synchronized,
    // because services are allowed to start and stop other services during their lifetime.
    synchronized<service_list_t> m_services;
    service_list_t m_unpublished;
    bool m_bootstrapped;

    // Context signalling hub.
    retroactive_signal<io::context_tag> m_signals;

    // Metrics.
    metrics::registry_t m_metrics_registry;

    std::unique_ptr<config_t> m_config;

    // Service port mapping and pinning.
    port_mapping_t m_mapper;

    std::unique_ptr<distributor<engine_pool_t>> m_engine_distributor;
public:
    context_impl_t(std::unique_ptr<config_t> _config,
                   std::unique_ptr<logging::logger_t> _log,
                   std::unique_ptr<api::repository_t> _repository) :
        m_log(new logging::trace_wrapper_t(std::move(_log))),
        m_repository(std::move(_repository)),
        m_bootstrapped(false),
        m_config(std::move(_config)),
        m_mapper(*m_config)
    {
        const holder_t scoped(*m_log, {{"source", "core"}});
        initialize_distributor();

        reset_logger_filter();

        COCAINE_LOG_INFO(m_log, "initializing the core");

        // Load the builtin plugins.
        essentials::initialize(*m_repository);

        // Load the rest of plugins.
        m_repository->load(m_config->path().plugins());

        m_acceptor_thread = std::make_unique<io::chamber_t>("acceptor", std::make_shared<io::io_service>());

        // Spin up all the configured services, launch execution units.
        COCAINE_LOG_INFO(m_log, "starting {:d} execution unit(s)", m_config->network().pool());

        while (m_pool.size() != m_config->network().pool()) {
            m_pool.emplace_back(std::make_unique<execution_unit_t>(*this));
        }

        COCAINE_LOG_INFO(m_log, "starting {:d} service(s)", m_config->services().size());

        try {
            m_config->services().each([&](const std::string& name, const config_t::component_t& service) mutable {
                const holder_t scoped(*m_log, {{"service", name}});

                COCAINE_LOG_DEBUG(m_log, "starting service");

                try {
                    insert(name, std::make_unique<tcp_actor_t>(*this, repository().get<api::service_t>(
                        service.type(),
                        *this,
                        m_acceptor_thread->get_io_service(),
                        name,
                        service.args()
                    )));
                } catch (const std::system_error& err) {
                    COCAINE_LOG_ERROR(m_log, "unable to initialize service: {}", error::to_string(err));
                    throw;
                } catch (const std::exception& err) {
                    COCAINE_LOG_ERROR(m_log, "unable to initialize service: {}", err.what());
                    throw;
                }
            });

            m_services.apply([&](service_list_t& list) {
                publish_all(list);
                m_bootstrapped = true;
            });
            COCAINE_LOG_DEBUG(m_log, "context is bootstrapped");
            m_signals.invoke<io::context::prepared>();
            COCAINE_LOG_DEBUG(m_log, "context is prepared");
        } catch (const std::exception& e) {
            COCAINE_LOG_ERROR(m_log, "emergency core shutdown: {}", e);
            terminate();
            std::throw_with_nested(cocaine::error_t("failed to start core"));
        }
    }

    ~context_impl_t() {
        const holder_t scoped(*m_log, {{"source", "core"}});

        // Signal and stop all the services, shut down execution units.
        terminate();
    }

    auto
    initialize_distributor() -> void {
        try {
            auto distributor_component = m_config->component_group("context").get("distributor");
            if(!distributor_component) {
                throw error_t("missing distributor component in context section");
            }
            m_engine_distributor = make_distributor<engine_pool_t>(distributor_component->type(), distributor_component->args());
        } catch (const std::exception& e) {
            COCAINE_LOG_WARNING(m_log, "could not load distributor config - {}; processing with default values", e);
            m_engine_distributor = make_distributor<engine_pool_t>("bucket_random", dynamic_t::object_t());
        }
    }

    std::unique_ptr<logging::logger_t>
    log(const std::string& source) override {
        return log(source, {});
    }

    std::unique_ptr<logging::logger_t>
    log(const std::string& source, blackhole::attributes_t attributes) override {
        attributes.push_back({"source", {source}});

        // TODO: Make it possible to use in-place operator+= to fill in more attributes?
        return std::make_unique<blackhole::wrapper_t>(*m_log, std::move(attributes));
    }

    void
    logger_filter(filter_t new_filter) override {
        m_log->filter(std::move(new_filter));
    }

    api::repository_t&
    repository() const override {
        return *m_repository;
    }

    retroactive_signal<io::context_tag>&
    signal_hub() override {
        return m_signals;
    }

    metrics::registry_t&
    metrics_hub() override {
        return m_metrics_registry;
    }

    const config_t&
    config() const override {
        return *m_config;
    }

    port_mapping_t&
    mapper() override {
        return m_mapper;
    }

    void
    insert(const std::string& name, std::unique_ptr<tcp_actor_t> service) override {
        insert_with(name, [&] {
            return std::move(service);
        });
    }

    void
    insert_with(const std::string& name, std::function<std::unique_ptr<tcp_actor_t>()> fn) override {
        const holder_t scoped(*m_log, {{"source", "core"}});

        m_services.apply([&](service_list_t& list) {
            auto it = std::find_if(list.begin(), list.end(), match{name});
            if(it != list.end()) {
                throw cocaine::error_t("service '{}' already exists", name);
            }

            auto service = fn();

            if (m_bootstrapped) {
                publish(name, std::move(service), list);
            } else {
                m_unpublished.emplace_back(name, std::move(service));
            }
        });
    }

    auto
    publish(std::string name, std::unique_ptr<tcp_actor_t> service, service_list_t& services) -> void {
        service->run();

        COCAINE_LOG_DEBUG(m_log, "service has been started", {
            {"service", name}
        });

        // Fire off the signal to alert concerned subscribers about the service starting event.
        m_signals.invoke<io::context::service::exposed>(service->prototype()->name(), std::forward_as_tuple(
            service->endpoints(),
            service->prototype()->version(),
            service->prototype()->root()
        ));

        services.emplace_back(std::move(name), std::move(service));
    }

    auto
    publish_all(service_list_t& services) -> void {
        for (auto it = std::begin(m_unpublished); it != std::end(m_unpublished);) {
            auto name = it->first;
            auto& service = it->second;
            publish(name, std::move(service), services);
            it = m_unpublished.erase(it);
        }
    }

    std::unique_ptr<tcp_actor_t>
    remove (const std::string& name) override {
        const holder_t scoped(*m_log, {{"source", "core"}});

        std::unique_ptr<tcp_actor_t> service;

        m_services.apply([&](service_list_t& list) {
            auto it = std::find_if(list.begin(), list.end(), match{name});
            if(it != list.end()) {
                service = std::move(it->second);
                list.erase(it);
            } else {
                throw cocaine::error_t("service '{}' doesn't exist", name);
            }
        });

        service->terminate();

        COCAINE_LOG_DEBUG(m_log, "service has been stopped", {
            { "service", name }
        });

        // Service is already terminated, so there's no reason to try to get its endpoints.
        std::vector<asio::ip::tcp::endpoint> nothing;

        // Fire off the signal to alert concerned subscribers about the service termination event.
        m_signals.invoke<io::context::service::removed>(service->prototype()->name(), std::forward_as_tuple(
            nothing,
            service->prototype()->version(),
            service->prototype()->root()
        ));

        return service;
    }

    boost::optional<context::quote_t>
    locate(const std::string& name) const override {
        return m_services.apply([&](const service_list_t& list) -> boost::optional<context::quote_t> {
            auto it = std::find_if(list.begin(), list.end(), match{name});
            if (it == list.end() || !it->second->is_active()) {
                return boost::none;
            }

            return boost::make_optional(context::quote_t{it->second->endpoints(), it->second->prototype()});
        });
    }

    std::map<std::string, context::quote_t>
    snapshot() const override {
        return m_services.apply([&](const service_list_t& list) {
            std::map<std::string, context::quote_t> result;
            for(auto& service_pair: list) {
                auto name = service_pair.first;
                const auto& actor = service_pair.second;
                result.emplace(std::move(name), context::quote_t{actor->endpoints(), actor->prototype()});
            }
            return result;
        });
    }

    execution_unit_t&
    engine() override {
        return *m_engine_distributor->next(m_pool);
    }

    void
    terminate() {
        COCAINE_LOG_INFO(m_log, "stopping {:d} service(s)", m_services->size());

        // Fire off to alert concerned subscribers about the shutdown. This signal happens before all
        // the outstanding connections are closed, so services have a chance to send their last wishes.
        m_signals.invoke<io::context::shutdown>();

        // Stop the service from accepting new clients or doing any processing. Pop them from the active
        // service list into this temporary storage, and then destroy them all at once. This is needed
        // because sessions in the execution units might still have references to the services, and their
        // lives have to be extended until those sessions are active.
        std::vector<std::unique_ptr<actor_t>> actors;

        m_config->services().each([&](const std::string& name, const config_t::component_t&){
            try {
                actors.push_back(remove(name));
            } catch (...) {
                // A service might be absent because it has failed to start during the bootstrap.
            }
        });

        // Do not wait for the service to finish all its stuff (like timers, etc). Graceful
        // termination happens only in engine chambers, because that's where client connections
        // are being handled.
        m_acceptor_thread->get_io_service().stop();

        // Does not block, unlike the one in execution_unit_t's destructors.
        m_acceptor_thread.reset();

        // There should be no outstanding services left. All the extra services spawned by others, like
        // app invocation services from the node service, should be dead by now.
        // TODO: Here's race by design. For example Node service listens `on_shutdown` signal, but
        // its invocation occurs in the `m_acceptor_thread` thread and can be missed due to force
        // I/O loop termination.

        // BOOST_ASSERT(m_services->empty());

        COCAINE_LOG_INFO(m_log, "stopping {:d} execution unit(s)", m_pool.size());
        m_pool.clear();

        // Destroy the service objects.
        actors.clear();

        reset_logger_filter();

        COCAINE_LOG_INFO(m_log, "core has been terminated");
    }

private:
    auto
    acceptor_loop() -> asio::io_service& override {
        return m_acceptor_thread->get_io_service();
    }

    auto
    reset_logger_filter() -> void {
        auto config_severity = m_config->logging().severity();
        auto filter = [=](filter_t::severity_t severity, filter_t::attribute_pack&) -> bool {
            return severity >= config_severity || trace_t::current().verbose();
        };
        logger_filter(filter_t(std::move(filter)));
    }
};

std::unique_ptr<context_t>
make_context(std::unique_ptr<config_t> config, std::unique_ptr<logging::logger_t> log) {
    std::unique_ptr<logging::logger_t> repository_logger(new blackhole::wrapper_t(*log, {}));
    std::unique_ptr<api::repository_t> repository(new api::repository_t(std::move(repository_logger)));
    return make_context(std::move(config), std::move(log), std::move(repository));
}

std::unique_ptr<context_t>
make_context(std::unique_ptr<config_t> config, std::unique_ptr<logging::logger_t> log, std::unique_ptr<api::repository_t> repository) {
    return std::unique_ptr<context_t>(new context_impl_t(std::move(config), std::move(log), std::move(repository)));
}

auto
context_t::uuid() const -> const std::string& {
    return config().uuid();
}

} //  namespace cocaine
