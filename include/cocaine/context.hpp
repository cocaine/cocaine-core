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

#ifndef COCAINE_CONTEXT_HPP
#define COCAINE_CONTEXT_HPP

#include "cocaine/common.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/repository.hpp"

#include <queue>

#include <boost/optional.hpp>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <blackhole/blackhole.hpp>

namespace cocaine {

// Configuration

struct config_t {
    config_t(const std::string& path);

    static
    int
    versions();

public:
    struct {
        std::string configuration;
        std::string plugins;
        std::string runtime;
    } path;

    struct {
        // Local hostname. It might be automatically detected or manually specified.
        std::string hostname;

        // An endpoint where all the services and the service locator will be bound.
        std::string endpoint;

        // I/O thread pool size.
        size_t pool;

        // Port mapper configuration.
        std::map<std::string, uint16_t> pinned;
        std::pair<uint16_t, uint16_t> shared;
    } network;

    struct logging_t {
        struct logger_t {
            logging::priorities verbosity;
            std::string timestamp;
            blackhole::log_config_t config;
        };

        std::map<std::string, logger_t> loggers;
    } logging;

    struct component_t {
        std::string type;
        dynamic_t args;
    };

    typedef std::map<std::string, component_t> component_map_t;

    component_map_t services;
    component_map_t storages;

#ifdef COCAINE_ALLOW_RAFT
    bool create_raft_cluster;
#endif
};

// Dynamic port mapper

class port_mapping_t {
    typedef std::uint16_t port_t;
    typedef std::priority_queue<port_t, std::vector<port_t>, std::greater<port_t>> queue_type;

    // Pinned service ports.
    std::map<std::string, port_t> m_pinned;

    // Ports available for dynamic allocation.
    queue_type m_shared;

public:
    port_mapping_t(const config_t& config);

    port_t
    assign(const std::string& name);

    void
    retain(const std::string& name, port_t port);
};

// Context

class actor_t;
class execution_unit_t;

class context_t {
    COCAINE_DECLARE_NONCOPYABLE(context_t)

    typedef std::deque<
        std::pair<std::string, std::unique_ptr<actor_t>>
    > service_list_t;

    // Service port mapping and pinning, synchronized using service list lock.
    port_mapping_t m_port_mapping;

    // TODO: There was an idea to use the Repository to enable pluggable sinks and whatever else for
    // for the Blackhole, when all the common stuff is extracted to a separate library.
    std::unique_ptr<logging::logger_t> m_logger;

    // NOTE: This is the first object in the component tree, all the other dynamic components, be it
    // storages or isolates, have to be declared after this one.
    std::unique_ptr<api::repository_t> m_repository;

    // A pool of execution units - threads responsible for doing all the service invocations.
    std::vector<std::unique_ptr<execution_unit_t>> m_pool;

    // Services are stored as a vector of pairs to preserve the initialization order. Synchronized,
    // because services are allowed to start and stop other services during their lifetime.
    synchronized<service_list_t> m_services;

#ifdef COCAINE_ALLOW_RAFT
    std::unique_ptr<raft::repository_t> m_raft;
#endif

public:
    const config_t config;

public:
    context_t(config_t config, const std::string& logger);
    context_t(config_t config, std::unique_ptr<logging::logger_t> logger);
   ~context_t();

    // Component API

    template<class Category, typename... Args>
    typename api::category_traits<Category>::ptr_type
    get(const std::string& type, Args&&... args);

    // Logging

    std::unique_ptr<logging::log_t>
    log(const std::string& source);

#ifdef COCAINE_ALLOW_RAFT
    auto
    raft() -> raft::repository_t& {
        return *m_raft;
    }
#endif

    // Services

    void
    insert(const std::string& name, std::unique_ptr<actor_t> service);

    auto
    remove(const std::string& name) -> std::unique_ptr<actor_t>;

    auto
    locate(const std::string& name) const -> boost::optional<actor_t&>;

    // I/O

    void
    attach(const std::shared_ptr<io::socket<io::tcp>>& ptr, const std::shared_ptr<const io::basic_dispatch_t>& dispatch);

private:
    void
    bootstrap();
};

template<class Category, typename... Args>
typename api::category_traits<Category>::ptr_type
context_t::get(const std::string& type, Args&&... args) {
    return m_repository->get<Category>(type, std::forward<Args>(args)...);
}

} // namespace cocaine

#endif
