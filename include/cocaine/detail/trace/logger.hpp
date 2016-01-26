/*
    Copyright (c) 2015-2016 Anton Matveenko <antmat@me.com>
    Copyright (c) 2015-2016 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_TRACE_LOGGER
#define COCAINE_TRACE_LOGGER

#include "cocaine/trace/trace.hpp"

#include <blackhole/attribute.hpp>
#include <blackhole/logger.hpp>

namespace cocaine { namespace logging {

class trace_wrapper_t :
    public blackhole::logger_t
{
    blackhole::logger_t& inner;

public:
    trace_wrapper_t(logger_t& log);

    auto attributes() const noexcept -> blackhole::attributes_t {
        if(!trace_t::current().empty()) {
            return trace_t::current().formatted_attributes<blackhole::attributes_t>();
        }

        return blackhole::attributes_t();
    }

    auto log(blackhole::severity_t severity, const blackhole::message_t& message) -> void;
    auto log(blackhole::severity_t severity, const blackhole::message_t& message, blackhole::attribute_pack& pack) -> void;
    auto log(blackhole::severity_t severity, const blackhole::lazy_message_t& message, blackhole::attribute_pack& pack) -> void;

    auto manager() -> blackhole::scope::manager_t&;
};

}} // namespace cocaine::logging

#endif // COCAINE_TRACE_LOGGER
