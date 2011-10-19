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
            std::string hostname;
            std::string instance;
            std::string route;
            std::vector<std::string> endpoints;
        } core;

        struct {
            std::string location;
        } downloads;

        struct engine_config_t {
            float suicide_timeout;
            float heartbeat_timeout;
            unsigned int queue_depth;
            unsigned int worker_limit;
            unsigned int history_depth;
        } engine;

        struct {
            std::string location;
        } registry;

        struct {
            std::string driver;
            std::string location;
        } storage;

    private:
        static config_t g_config;
};

}

#endif
