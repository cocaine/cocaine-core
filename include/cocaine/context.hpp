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

#include <boost/format.hpp>
#include <boost/thread/mutex.hpp>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine {

struct defaults {
    // Default profile.
    static const char slave[];
    static const float heartbeat_timeout;
    static const float idle_timeout;
    static const float startup_timeout;
    static const float termination_timeout;
    static const unsigned long pool_limit;
    static const unsigned long queue_limit;

    // Default I/O policy.
    static const long bus_timeout;
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

    typedef struct {
        std::string type;
        Json::Value args;
    } component_info_t;

#if BOOST_VERSION >= 103600
    typedef boost::unordered_map<
#else
    typedef std::map<
#endif
        std::string,
        component_info_t
    > component_info_map_t;

    component_info_map_t components;

    struct {
        std::string hostname;
    } runtime;
};

class context_t:
    public boost::noncopyable
{
    public:
        context_t(config_t config,
                  boost::shared_ptr<logging::sink_t> sink);

        ~context_t();

        // Component API
        // -------------
        
        template<class Category>
        typename api::category_traits<Category>::ptr_type
        get(const std::string& type,
            const typename api::category_traits<Category>::args_type& args)
        {
            return m_repository->get<Category>(type, args);
        }

        template<class Category>
        typename api::category_traits<Category>::ptr_type
        get(const std::string& name) {
            config_t::component_info_map_t::const_iterator it(
                config.components.find(name)
            );
    
            if(it == config.components.end()) {
                boost::format message("the '%s' component is not configured");
                throw configuration_error_t((message % name).str());
            }

            return get<Category>(
                it->second.type,
                typename api::category_traits<Category>::args_type(
                    name,
                    it->second.args
                )
            );
        }

        // Networking
        // ----------

        zmq::context_t&
        io() {
            return *m_io;
        }
        
        // Logging
        // -------

        boost::shared_ptr<logging::logger_t>
        log(const std::string&);

    public:
        const config_t config;

    private:
        boost::shared_ptr<logging::sink_t> m_sink;

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
        std::unique_ptr<api::repository_t> m_repository;
};

} // namespace cocaine

#endif
