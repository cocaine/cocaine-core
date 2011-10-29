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
            std::vector<std::string> endpoints;
            std::string hostname;
            std::string instance;
        } core;

        struct {
            std::string location;
        } downloads;

        struct engine_cfg_t {
            std::string backend;
            float heartbeat_timeout;
            float suicide_timeout;
            unsigned int pool_limit;
            unsigned int queue_limit;
        } engine;

        struct {
            std::string location;
        } registry;

        struct {
            std::string driver;
            std::string location;
        } storage;

    private:
        config_t();

        static config_t g_config;
};

}

#endif
