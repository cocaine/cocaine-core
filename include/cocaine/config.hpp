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
