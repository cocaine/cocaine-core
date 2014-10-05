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

#include "cocaine/dynamic/dynamic.hpp"

#include "cocaine/locked_ptr.hpp"
#include "cocaine/repository.hpp"

#include <blackhole/blackhole.hpp>

#include <boost/asio/ip/address.hpp>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/optional.hpp>
#include <boost/signals2/signal.hpp>

namespace cocaine {

// Configuration

struct config_t {
    config_t(const std::string& source);

    static
    int
    versions();

public:
    struct {
        std::string plugins;
        std::string runtime;
    } path;

    struct {
        // An endpoint where all the services will be bound. Note that binding on [::] will bind on
        // 0.0.0.0 too as long as the "net.ipv6.bindv6only" sysctl is set to 0 (default).
        boost::asio::ip::address endpoint;

        // Local hostname. In case it can't be automatically detected by resolving a CNAME for the
        // contents of /etc/hostname via the default system resolver, it can be configured manually.
        std::string hostname;

        // I/O thread pool size.
        size_t pool;

        struct {
            // Pinned ports for static service port allocation.
            std::map<std::string, port_t> pinned;

            // Port range to populate the dynamic port pool for service port allocation.
            std::tuple<port_t, port_t> shared;
        } ports;
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
        dynamic_t   args;
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
    // Pinned service ports.
    std::map<std::string, port_t> const m_pinned;

    // Ports available for dynamic allocation.
    std::deque<port_t> m_shared;
    std::mutex m_mutex;

    // Ports currently in use.
    std::map<std::string, port_t> m_in_use;

public:
    explicit
    port_mapping_t(const config_t& config);

    // Modifiers

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

    typedef std::deque<std::pair<std::string, std::unique_ptr<actor_t>>> service_list_t;

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

    // Service port mapping and pinning.
    port_mapping_t mapper;

    struct signals_t {
        typedef boost::signals2::signal<void()> context_signals_t;
        typedef boost::signals2::signal<void(const actor_t& service)> service_signals_t;

        // Fired first thing on context shutdown. This is a very good time to cleanup persistent
        // connections, synchronize disk state and so on.
        context_signals_t shutdown;

        struct {
            // Fired on service creation, after service's thread is launched and is ready to accept
            // and process new incoming connections.
            service_signals_t exposed;

            // Fired on service destruction, after the service was removed from its endpoints, but
            // before the service object is actually destroyed.
            service_signals_t removed;
        } service;
    };

    // Lifecycle management signals.
    signals_t signals;

public:
    context_t(config_t config, const std::string& logger);
    context_t(config_t config, std::unique_ptr<logging::logger_t> logger);
   ~context_t();

    std::unique_ptr<logging::log_t>
    log(const std::string& source, blackhole::log::attributes_t = blackhole::log::attributes_t());

    template<class Category, typename... Args>
    typename api::category_traits<Category>::ptr_type
    get(const std::string& type, Args&&... args) const;

    // Service API

    void
    insert(const std::string& name, std::unique_ptr<actor_t> service);

    auto
    remove(const std::string& name) -> std::unique_ptr<actor_t>;

    auto
    locate(const std::string& name) const -> boost::optional<const actor_t&>;

    // Network I/O

    auto
    engine() -> execution_unit_t&;

    // Raft

#ifdef COCAINE_ALLOW_RAFT
    auto
    raft() -> raft::repository_t& {
        return *m_raft;
    }
#endif

private:
    void
    bootstrap();
};

template<class Category, typename... Args>
typename api::category_traits<Category>::ptr_type
context_t::get(const std::string& type, Args&&... args) const {
    return m_repository->get<Category>(type, std::forward<Args>(args)...);
}

} // namespace cocaine

#endif
