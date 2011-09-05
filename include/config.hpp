#ifndef YAPPI_CONFIG_HPP
#define YAPPI_CONFIG_HPP

#include "common.hpp"

namespace yappi {

class config_t {
    public:
        static const config_t& get();
        static config_t& set();

    public:
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
            std::string plugins;
            std::string storage;
        } paths;

        struct {
            float suicide_timeout;
            float collect_timeout;
        } engine;

        struct {
            bool disabled;
        } storage;

        struct {
            unsigned int protocol;
            uint32_t history_depth;
        } core;

    private:
        static config_t g_config;
};

}

#endif
