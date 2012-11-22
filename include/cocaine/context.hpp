/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include <queue>

#include <boost/thread/mutex.hpp>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine {

struct defaults {
    // Default slave.
    static const char slave[];
    
    // Default profile.
    static const float heartbeat_timeout;
    static const float idle_timeout;
    static const float startup_timeout;
    static const float termination_timeout;
    static const unsigned long pool_limit;
    static const unsigned long queue_limit;
    static const unsigned long concurrency;

    // Default I/O policy.
    static const long control_timeout;
    static const unsigned long io_bulk_size;

    // Default paths.
    static const char ipc_path[];
    static const char plugin_path[];
    static const char spool_path[];
};

struct config_t {
    config_t(const std::string& config_path);

    std::string config_path,
                ipc_path,
                plugin_path,
                spool_path;

    struct component_t {
        std::string type;
        Json::Value args;
    };

#if BOOST_VERSION >= 103600
    typedef boost::unordered_map<
#else
    typedef std::map<
#endif
        std::string,
        component_t
    > component_map_t;

    static
    component_map_t
    parse(const Json::Value& config);

    // NOTE: A configuration map for the generic components, like storages or loggers,
    // which are specified in the configuration file.
    component_map_t components;

    struct {
        std::string hostname;
        std::pair<uint16_t, uint16_t> ports;
    } network;
};

// Free port dispenser for automatic socket binding.

struct port_mapper_t {
    port_mapper_t(const std::pair<uint16_t, uint16_t>& limits);

    uint16_t
    get();

    void
    retain(uint16_t port);

private:
    std::priority_queue<
        uint16_t,
        std::vector<uint16_t>,
        std::greater<uint16_t>
    > m_ports;
    
    boost::mutex m_mutex;
};

class context_t:
    public boost::noncopyable
{
    public:
        context_t(config_t config);
        ~context_t();

        // Logging

        boost::shared_ptr<logging::logger_t>
        log(const std::string&);
        
        // Component API
        
        template<class Category, typename... Args>
        typename api::category_traits<Category>::ptr_type
        get(const std::string& type,
            Args&&... args);

        template<class Category>
        typename api::category_traits<Category>::ptr_type
        get(const std::string& name);

        // Networking

        zmq::context_t&
        io() {
            return *m_io;
        }

        port_mapper_t&
        ports() {
            return *m_port_mapper;
        }

    public:
        const config_t config;

    private:
        std::unique_ptr<api::repository_t> m_repository;
        
        // NOTE: As the logging sinks themselves are components, the repository
        // have to be initialized first without a logger, unfortunately.
        std::unique_ptr<api::sink_t> m_sink;

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            std::string,
            boost::shared_ptr<logging::logger_t>
        > instance_map_t;

        instance_map_t m_instances;
        boost::mutex m_mutex;

        std::unique_ptr<zmq::context_t> m_io;
        
        // TODO: I don't really like this implementation.
        std::unique_ptr<port_mapper_t> m_port_mapper;
};

template<class Category, typename... Args>
typename api::category_traits<Category>::ptr_type
context_t::get(const std::string& type,
               Args&&... args)
{
    return m_repository->get<Category>(type, std::forward<Args>(args)...);
}

template<class Category>
typename api::category_traits<Category>::ptr_type
context_t::get(const std::string& name) {
    config_t::component_map_t::const_iterator it(
        config.components.find(name)
    );

    if(it == config.components.end()) {
        throw configuration_error_t("the '%s' component is not configured", name);
    }

    return get<Category>(
        it->second.type,
        *this,
        name,
        it->second.args
    );
}

} // namespace cocaine

#endif
