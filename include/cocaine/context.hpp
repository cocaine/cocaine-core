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

#ifndef COCAINE_CONTEXT_HPP
#define COCAINE_CONTEXT_HPP

#include "cocaine/common.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/repository.hpp"

#include <queue>

#include <boost/optional.hpp>

namespace cocaine {

struct defaults {
    // Default profile.
    static const bool log_output;
    static const float heartbeat_timeout;
    static const float idle_timeout;
    static const float startup_timeout;
    static const float termination_timeout;
    static const unsigned long pool_limit;
    static const unsigned long queue_limit;
    static const unsigned long concurrency;
    static const unsigned long crashlog_limit;

    // Default I/O policy.
    static const float control_timeout;
    static const unsigned decoder_granularity;

    // Default paths.
    static const char plugins_path[];
    static const char runtime_path[];

    // Defaults for service locator.
    static const char endpoint[];
    static const uint16_t locator_port;
    static const uint16_t min_port;
    static const uint16_t max_port;
};

// Configuration

struct config_t {
    config_t(const std::string& config_path);

    struct {
        std::string config;
        std::string plugins;
        std::string runtime;
    } path;

    struct component_t {
        std::string type;
        dynamic_t   args;
    };

    struct {
        std::string hostname;
        std::string uuid;

        // NOTE: An endpoint where all the services and the service locator will be bound.
        std::string endpoint;

        // NOTE: Service locator port is configurable to allow multiple runtimes to run on a single
        // machine. This port will be forwarded to the slaves via a command-line argument.
        uint16_t    locator;

        boost::optional<std::string> group;
        boost::optional<std::tuple<uint16_t, uint16_t>> ports;
        boost::optional<component_t> gateway;
    } network;

    struct {
        std::set<std::pair<std::string, uint16_t>> some_nodes;
        std::string service_name;
        std::string config_machine_name;
        unsigned int election_timeout;
        unsigned int heartbeat_timeout;
        unsigned int snapshot_threshold;
        unsigned int message_size;
        bool create_configuration_cluster;
        bool enable;
    } raft;

    typedef std::map<std::string, component_t> component_map_t;

    component_map_t loggers;
    component_map_t services;
    component_map_t storages;

public:
    static
    int
    version();
};

// Context

class actor_t;
class execution_unit_t;

template<class T>
struct reverse_priority_queue {
    typedef std::priority_queue<T, std::vector<T>, std::greater<T>> type;
};

class context_t {
    COCAINE_DECLARE_NONCOPYABLE(context_t)

    // NOTE: This is the first object in the component tree, all the other components, including
    // loggers, storages or isolates have to be declared after this one.
    std::unique_ptr<api::repository_t> m_repository;

    // NOTE: As the loggers themselves are components, the repository has to be initialized
    // first without a logger, unfortunately.
    std::unique_ptr<logging::logger_concept_t> m_logger;

    // Ports available for allocation.
    reverse_priority_queue<uint16_t>::type m_ports;

    typedef std::deque<
        std::pair<std::string, std::unique_ptr<actor_t>>
    > service_list_t;

    // These are the instances of all the configured services, stored as a vector of pairs to
    // preserve the initialization order. Synchronized, because services are allowed to start
    // and stop other services during their lifetime.
    synchronized<service_list_t> m_services;

    // A pool of execution units - threads responsible for doing all the service invocations.
    std::vector<std::unique_ptr<execution_unit_t>> m_pool;

    struct synchronization_t;

    // Synchronization object is responsible for tracking remote clients and sending them service
    // configuration updates when necessary.
    std::shared_ptr<synchronization_t> m_synchronization;

public:
    const config_t config;

    std::unique_ptr<raft::repository_t> raft;

public:
    context_t(config_t config, const std::string& logger);
    context_t(config_t config, std::unique_ptr<logging::logger_concept_t>&& logger);
   ~context_t();

    // Component API

    template<class Category, typename... Args>
    typename api::category_traits<Category>::ptr_type
    get(const std::string& type, Args&&... args);

    // Logging

    auto
    logger() -> logging::logger_concept_t& {
        return *m_logger;
    }

    // Services

    void
    insert(const std::string& name, std::unique_ptr<actor_t>&& service);

    auto
    remove(const std::string& name) -> std::unique_ptr<actor_t>;

    auto
    locate(const std::string& name) const -> boost::optional<actor_t&>;

    // I/O

    void
    attach(const std::shared_ptr<io::socket<io::tcp>>& ptr, const std::shared_ptr<io::dispatch_t>& dispatch);

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
