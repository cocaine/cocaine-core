#ifndef YAPPI_CONFIG_HPP
#define YAPPI_CONFIG_HPP

#include "common.hpp"

namespace yappi {

struct config_t {
    struct {
        std::vector<std::string> listen;
        std::vector<std::string> publish;
        uint64_t watermark;
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
#if BOOST_VERSION > 103500
        uint32_t history_depth;
#endif
    } core;
};

}

#endif
