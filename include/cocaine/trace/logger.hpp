#pragma once

#include <cocaine/forwards.hpp>
#include <blackhole/attribute.hpp>

namespace cocaine { namespace tracer {

class attribute_scope_t {
public:
    virtual ~attribute_scope_t() = 0;
};

inline attribute_scope_t::~attribute_scope_t() { }

class logger_t {
private:
    virtual void log_impl(std::string message, blackhole::attribute::set_t attributes) = 0;
public:
    //Sorry pals, only formatted message here
    virtual
    inline void log(std::string message, blackhole::attribute::set_t attributes);

    inline void log(std::string message);

    virtual
    std::unique_ptr<attribute_scope_t>
    get_scope(blackhole::attribute::set_t attributes) const = 0;

    virtual inline
    ~logger_t() {}
};

}}

#include "cocaine/trace/trace.hpp"

namespace cocaine { namespace tracer {

void logger_t::log(std::string message, blackhole::attribute::set_t attributes) {
    if(current_span()->should_log()) {
        log_impl(std::move(message), std::move(attributes));
    }
}

void logger_t::log(std::string message) {
    log(std::move(message), blackhole::attribute::set_t());
}

}}
