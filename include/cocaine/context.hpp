//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#include <syslog.h>

#include "cocaine/common.hpp"

namespace cocaine {

struct config_t {
    struct {
        // Plugin path
        std::string plugins;

        // Administration and routing
        std::vector<std::string> endpoints;
        std::string hostname;
        std::string instance;
        
        // Automatic discovery
        std::string announce_endpoint;
        float announce_interval;
    } core;

    struct {
        // Default engine policy
        std::string backend;
        float heartbeat_timeout;
        float suicide_timeout;
        unsigned int pool_limit;
        unsigned int queue_limit;
    } engine;

    struct {
        // Storage type and path
        std::string driver;
        std::string location;
    } storage;
};

class logger_t {
    public:
        logger_t() { }
        logger_t(logger_t& other, const std::string& prefix) { }

        virtual void emit(int priority, const char* format, ...) { };
};

class context_t:
    public boost::noncopyable
{
    public:
        context_t(config_t config);

        inline zmq::context_t& io() {
            if(!m_io) {
                throw std::runtime_error("networking is not initialized");
            }

            return *m_io;
        }

        inline logger_t& log() {
            if(!m_log) {
                throw std::runtime_error("logging is not initialized");
            }

            return *m_log;
        }

    public:
        config_t config;

    private:
        boost::shared_ptr<zmq::context_t> m_io;
        boost::shared_ptr<logger_t> m_log;
};

}

#endif
