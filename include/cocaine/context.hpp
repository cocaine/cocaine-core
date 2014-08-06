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

#ifdef COCAINE_ALLOW_RAFT
    bool create_raft_cluster;
#endif

    typedef std::map<std::string, component_t> component_map_t;

    component_map_t services;
    component_map_t storages;

    struct logging_t {
        struct logger_t {
            logging::priorities verbosity;
            std::string timestamp;
            blackhole::log_config_t config;
        };

        std::map<std::string, logger_t> loggers;
    } logging;

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

    // TODO: There was an idea to use the Repository to enable pluggable sinks and whatever else
    // for the Blackhole, when all the common stuff is extracted to a separate library.
    std::unique_ptr<logging::logger_t> m_logger;

    // NOTE: This is the first object in the component tree, all the other components, including
    // storages or isolates have to be declared after this one.
    std::unique_ptr<api::repository_t> m_repository;

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

#ifdef COCAINE_ALLOW_RAFT
    std::unique_ptr<raft::repository_t> m_raft;
#endif

public:
    const config_t config;

public:
    context_t(config_t config, const std::string& logger);
    context_t(config_t config, std::unique_ptr<logging::logger_t>&& logger);
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
    insert(const std::string& name, std::unique_ptr<actor_t>&& service);

    auto
    remove(const std::string& name) -> std::unique_ptr<actor_t>;

    auto
    locate(const std::string& name) const -> boost::optional<actor_t&>;

    // I/O

    void
    attach(const std::shared_ptr<io::socket<io::tcp>>& ptr, const std::shared_ptr<io::basic_dispatch_t>& dispatch);

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
