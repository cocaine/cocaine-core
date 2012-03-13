//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef COCAINE_CONTEXT_HPP
#define COCAINE_CONTEXT_HPP

#include <boost/thread/mutex.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

#include "cocaine/registry.hpp"

namespace cocaine {

struct config_t {
    struct {
        // Plugin path.
        std::string modules;

        // Administration and routing.
        std::vector<std::string> endpoints;
        std::string hostname;
        std::string instance;
        
        // Automatic discovery.
        std::string announce_endpoint;
        float announce_interval;

        // Control groups availability.
        bool cgroups;
    } core;

    struct {
        // Default engine policy.
        float heartbeat_timeout;
        float suicide_timeout;
        unsigned int pool_limit;
        unsigned int queue_limit;
    } defaults;

    struct {
        // Storage type and path.
        std::string driver;
        std::string uri;
    } storage;
};

class context_t {
    public:
        context_t(config_t config, std::auto_ptr<logging::sink_t> sink);
        context_t(config_t config);

        // Copying is allowed, will reinitialize some subsystems.
        context_t(const context_t& other);
        context_t& operator=(const context_t& other);

        // Returns a possibly cached logger with the specified name.
        boost::shared_ptr<logging::logger_t> log(const std::string& type);

        template<class Category>
        std::auto_ptr<Category> create(const std::string& type) {
            return m_registry->create<Category>(type);
        }

        zmq::context_t& io();
        storages::storage_t& storage();

    private:
        void initialize();

    public:
        config_t config;

    private:
        // Initialization interlocking.
        boost::mutex m_mutex;

        // Core subsystems.
        boost::shared_ptr<logging::sink_t> m_sink;
        boost::shared_ptr<core::registry_t> m_registry;
        boost::shared_ptr<zmq::context_t> m_io;
        boost::shared_ptr<storages::storage_t> m_storage;
};

}

#endif
