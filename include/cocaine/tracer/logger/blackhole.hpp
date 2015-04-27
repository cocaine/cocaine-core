#pragma once

#include <cocaine/context.hpp>
#include "cocaine/trace/logger.hpp"
namespace cocaine { namespace tracer {

class bh_atttribute_scope_t : public attribute_scope_t {
public:
    bh_atttribute_scope_t(logging::logger_t& logger, blackhole::attribute::set_t attributes) :
        scope(logger, std::move(attributes))
    {}

    virtual
    ~bh_atttribute_scope_t() {}
private:
    blackhole::scoped_attributes_t scope;
};


class blackhole_logger_t : public logger_t {
public:
    /*
     * Valid only when logger is in scope, so lifetime should be controlled manually.
     */
    inline
    blackhole_logger_t(logging::logger_t& _logger) :
        logger(_logger)
    {
    }

    virtual
    void
    log_impl(std::string message, blackhole::attribute::set_t attributes) {
        if(auto record = logger.open_record(::cocaine::logging::info, std::move(attributes))) {
            ::blackhole::aux::logger::make_pusher(logger, record, message.c_str());
        }
    }

    virtual
    std::unique_ptr<attribute_scope_t>
    get_scope(blackhole::attribute::set_t attributes) const {
        return std::unique_ptr<attribute_scope_t>(new bh_atttribute_scope_t(logger, std::move(attributes)));
    }

private:
    logging::logger_t& logger;
};

}}