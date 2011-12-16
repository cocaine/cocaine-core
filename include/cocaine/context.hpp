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

#include "cocaine/common.hpp"
#include "cocaine/networking.hpp"

namespace cocaine {

struct config_t {
    struct {
        // Administration and routing
        std::vector<std::string> endpoints;
        std::string hostname;
        std::string instance;
        
        // Automatic discovery
        std::string announce_endpoint;
        float announce_interval;
    } core;

    struct engine_cfg_t {
        // Default engine policy
        std::string backend;
        float heartbeat_timeout;
        float suicide_timeout;
        unsigned int pool_limit;
        unsigned int queue_limit;
    } engine;

    struct {
        // Plugin path
        std::string location;
    } registry;

    struct {
        // Storage type and path
        std::string driver;
        std::string location;
    } storage;
};

class context_t:
    boost::noncopyable
{
    public:
        context_t(config_t config_):
            config(config_),
            bus(new zmq::context_t(1))
        { }

    public:
        config_t config;
        boost::shared_ptr<zmq::context_t> bus;
};

}

#endif
