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
#include <deque>
#include <msgpack.hpp>

#include "cocaine/common.hpp"

#include "cocaine/registry.hpp"

namespace cocaine {

struct config_t {
    config_t();

    struct {
        // Administration socket endpoint.
        std::vector<std::string> endpoints;
        
        // Automatic discovery.
        std::string announce_endpoint;
        float announce_interval;
    } core;

    struct {
        std::string id;
        std::string app;
    } slave;

    struct engine_config_t {
        // Default engine policy.
        float heartbeat_timeout;
        float suicide_timeout;
        unsigned int pool_limit;
        unsigned int queue_limit;

        // I/O bulk size
        unsigned int io_bulk_size;

        MSGPACK_DEFINE(heartbeat_timeout, suicide_timeout, pool_limit, queue_limit);
    } defaults;

    struct registry_config_t {
        // Module path.
        std::string modules;
        
        MSGPACK_DEFINE(modules);
    } registry;

    struct storage_config_t {
        // Storage type and path.
        std::string driver;
        std::string uri;

        MSGPACK_DEFINE(driver, uri);
    } storage;
    
    struct {
        std::string self;
        std::string hostname;

        // Usable port range
        std::deque<uint16_t> ports;
    } runtime;

    // Logging sink.
    boost::shared_ptr<logging::sink_t> sink;
    
    MSGPACK_DEFINE(defaults, registry, storage);
};

class context_t:
    public boost::noncopyable
{
    public:
        context_t(config_t config);
        ~context_t();

        zmq::context_t& io();
        
        // Returns a possibly cached logger with the specified name.
        boost::shared_ptr<logging::logger_t> log(const std::string& type);

        template<class Category>
        std::auto_ptr<Category> create(const std::string& type) {
            return m_registry->create<Category>(type);
        }

        storages::storage_t& storage();

    public:
        config_t config;

    private:
        // Initialization interlocking.
        boost::mutex m_mutex;

        // Core subsystems.
        std::auto_ptr<zmq::context_t> m_io;
        std::auto_ptr<core::registry_t> m_registry;
        std::auto_ptr<storages::storage_t> m_storage;
};

}

#endif
