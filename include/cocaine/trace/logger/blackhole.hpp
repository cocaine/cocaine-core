#pragma once

#include <cocaine/context.hpp>
#include "cocaine/tracer/logger.hpp"
namespace cocaine { namespace tracer {

class blackhole_logger_t : public logger_t {
public:
    blackhole_logger_t(cocaine::context_t& context) : {

    }
    virtual void log(std::string message, blackhole::attribute_set_t attributes) = 0;
};
}}