/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/detail/runtime/logging.hpp"

BLACKHOLE_BEG_NS

namespace sink {

// Mapping trait that is called by Blackhole each time when syslog mapping is required

priority_t
priority_traits<cocaine::logging::priorities>::map(cocaine::logging::priorities level) {
    switch (level) {
    case cocaine::logging::debug:
        return priority_t::debug;
    case cocaine::logging::info:
        return priority_t::info;
    case cocaine::logging::warning:
        return priority_t::warning;
    case cocaine::logging::error:
        return priority_t::err;
    default:
        return priority_t::debug;
    }

    return priority_t::debug;
}

} // namespace sink

BLACKHOLE_END_NS

// Severity attribute converter from enumeration underlying type into string

namespace cocaine { namespace logging {

void
map_severity(blackhole::aux::attachable_ostringstream& stream, const logging::priorities& level) {
    typedef blackhole::aux::underlying_type<logging::priorities>::type underlying_type;

    static const std::array<const char*, 4> describe = {{ "D", "I", "W", "E" }};

    const size_t value = static_cast<size_t>(level);

    if(value < describe.size()) {
        stream << describe[value];
    } else {
        stream << value;
    }
}

}} // namespace cocaine::logging
