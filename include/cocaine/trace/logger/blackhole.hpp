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

#ifndef COCAINE_TRACE_LOGGER_BLACKHOLE
#define COCAINE_TRACE_LOGGER_BLACKHOLE

#include "cocaine/trace/trace.hpp"

#include <blackhole/logger/wrapper.hpp>

namespace cocaine { namespace logging {

struct trace_attribute_fetcher_t {
    blackhole::attribute::set_t
    operator()() const {
        if(!trace_t::current().empty()) {
            return trace_t::current().attributes<blackhole::attribute::set_t>();
        }
        return blackhole::attribute::set_t();
    }
};

template<class Wrapped, class AttributeFetcher>
class dynamic_wrapper_t:
public blackhole::wrapper_base_t<Wrapped>
{
    static_assert(std::is_same<
                  typename blackhole::unwrap<Wrapped>::logger_type,
                  blackhole::verbose_logger_t<typename blackhole::unwrap<Wrapped>::logger_type::level_type>
              >::value, "Invalid logger type for dynamic wrapper");
    typedef blackhole::wrapper_base_t<Wrapped> base_type;

public:
    typedef typename base_type::underlying_type underlying_type;

public:
    dynamic_wrapper_t(underlying_type& wrapped, blackhole::attribute::set_t attributes) :
        base_type(wrapped, std::move(attributes))
    {}

    dynamic_wrapper_t(const dynamic_wrapper_t& wrapper, const blackhole::attribute::set_t& attributes) :
        base_type(wrapper, attributes)
    {}

    dynamic_wrapper_t(dynamic_wrapper_t&& other) :
        base_type(std::move(other))
    {}

    dynamic_wrapper_t& operator=(dynamic_wrapper_t&& other) {
        base_type::operator =(std::move(other));
        return *this;
    }

    template<typename Level>
    blackhole::record_t
    open_record(Level level, blackhole::attribute::set_t attributes = blackhole::attribute::set_t()) const {
        // TODO: Do this under lock or drop assignment.
        AttributeFetcher fetcher;
        const auto& dynamic_attributes = fetcher();
        std::copy(this->attributes.begin(), this->attributes.end(), std::back_inserter(attributes));
        std::copy(dynamic_attributes.begin(), dynamic_attributes.end(), std::back_inserter(attributes));
        return this->wrapped->open_record(level, std::move(attributes));
    }
};

}} // namespace logging // namespace cocaine

#endif // COCAINE_TRACE_LOGGER_BLACKHOLE
