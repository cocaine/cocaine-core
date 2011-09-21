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
            std::string instance;
            uint32_t history_depth;
        } core;

        struct {
            std::vector<std::string> listen;
            std::vector<std::string> publish;
#if ZMQ_VERSION > 30000
            int watermark;
#else
            uint64_t watermark;
#endif
        } net;
        
        struct {
            std::string path;
        } registry;

        struct {
            float suicide_timeout;
            float collect_timeout;
#if BOOST_VERSION > 103500
            float linger_timeout;
#endif
        } engine;

        struct {
            std::string driver;
            std::string location;
        } storage;

    private:
        static config_t g_config;
};

}

#endif
