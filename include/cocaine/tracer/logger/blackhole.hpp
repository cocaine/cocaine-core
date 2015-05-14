/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@me.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser Gene ral Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_TRACER_LOGGER_BLACKHOLE
#define COCAINE_TRACER_LOGGER_BLACKHOLE

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

#endif
