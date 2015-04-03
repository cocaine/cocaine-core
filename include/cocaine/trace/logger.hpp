#pragma once

#include <cocaine/forwards.hpp>

namespace cocaine { namespace tracer {
class logger_t {
public:
    //Sorry pals, only formatted message here
    virtual void log(std::string message, blackhole::attribute_set_t attributes) = 0;
    void register_self() {

    }
    void unregister_self();
};
}}