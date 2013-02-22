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

#include "cocaine/common.hpp"
#include "cocaine/json.hpp"
#include "cocaine/repository.hpp"

namespace cocaine {

struct defaults {
    // Default profile.
    static const float heartbeat_timeout;
    static const float idle_timeout;
    static const float startup_timeout;
    static const float termination_timeout;
    static const unsigned long pool_limit;
    static const unsigned long queue_limit;
    static const unsigned long concurrency;

    // Default I/O policy.
    static const float control_timeout;

    // Default paths.
    static const char plugins_path[];
    static const char runtime_path[];
    static const char spool_path[];
};

// Configuration

struct config_t {
    config_t(const std::string& config_path);

    struct {
        std::string config;
        std::string plugins;
        std::string runtime;
        std::string spool;
    } path;

    struct {
        std::string hostname;
    } network;

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

    component_map_t storages;
    component_map_t loggers;
    component_map_t services;

public:
    static
    component_map_t
    parse(const Json::Value& config);
};

// Context

class context_t:
    boost::noncopyable
{
    public:
        context_t(config_t config,
                  const std::string& logger);

        context_t(config_t config,
                  std::unique_ptr<logging::logger_t>&& logger);

        ~context_t();

        // Component API

        template<class Category, typename... Args>
        typename api::category_traits<Category>::ptr_type
        get(const std::string& type,
            Args&&... args);

        // Logging

        logging::logger_t&
        logger() {
            return *m_logger;
        }

    private:
        void
        initialize();

    public:
        const config_t config;

    private:
        // NOTE: This is the first object in the component tree, all the other
        // components, including loggers, storages or isolates have to be declared
        // after this one.
        std::unique_ptr<api::repository_t> m_repository;

        // NOTE: As the loggers themselves are components, the repository
        // have to be initialized first without a logger, unfortunately.
        std::unique_ptr<logging::logger_t> m_logger;
};

template<class Category, typename... Args>
typename api::category_traits<Category>::ptr_type
context_t::get(const std::string& type,
               Args&&... args)
{
    return m_repository->get<Category>(
        type,
        std::forward<Args>(args)...
    );
}

} // namespace cocaine

#endif
