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

#ifndef COCAINE_CONFIG_HPP
#define COCAINE_CONFIG_HPP

#include "cocaine/common.hpp"

namespace cocaine {

class config_t {
    public:
        static const config_t& get();
        static config_t& set();

    public:
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

    private:
        config_t();

    private:
        static config_t g_config;
};

}

#endif
