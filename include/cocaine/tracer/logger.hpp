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

#ifndef COCAINE_TRACER_LOGGER
#define COCAINE_TRACER_LOGGER

#include <cocaine/forwards.hpp>
#include <blackhole/attribute.hpp>

namespace cocaine { namespace tracer {

class attribute_scope_t {
public:
    virtual ~attribute_scope_t() = 0;
};

inline
attribute_scope_t::~attribute_scope_t() { }

class logger_t {
private:
    virtual
    void
    log_impl(std::string message, blackhole::attribute::set_t attributes) = 0;
public:
    //Sorry pals, only formatted message here
    virtual
    inline
    void
    log(std::string message, blackhole::attribute::set_t attributes);

    inline
    void
    log(std::string message);

    virtual
    std::unique_ptr<attribute_scope_t>
    get_scope(blackhole::attribute::set_t attributes) const = 0;

    virtual
    inline
    ~logger_t() {}
};

}}

#include "cocaine/trace/trace.hpp"

namespace cocaine { namespace tracer {

void logger_t::log(std::string message, blackhole::attribute::set_t attributes) {
    if(!current_span()->empty()) {
        log_impl(std::move(message), std::move(attributes));
    }
}

void logger_t::log(std::string message) {
    log(std::move(message), blackhole::attribute::set_t());
}

}}

#endif
