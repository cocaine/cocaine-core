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

#ifndef COCAINE_BOOTSTRAP_LOGGING_HPP
#define COCAINE_BOOTSTRAP_LOGGING_HPP

#include "cocaine/common.hpp"
#include "cocaine/dynamic/dynamic.hpp"
#include "cocaine/logging.hpp"

// Blackhole support

#include <blackhole/repository/config/parser.hpp>

BLACKHOLE_BEG_NS

namespace repository { namespace config {

template<>
struct transformer_t<cocaine::dynamic_t> {
    typedef cocaine::dynamic_t value_type;

    static
    dynamic_t
    transform(const value_type& value);
};

}} // namespace repository::config

BLACKHOLE_END_NS

#include <blackhole/frontend/syslog.hpp>

BLACKHOLE_BEG_NS

namespace sink {

template<>
struct priority_traits<cocaine::logging::priorities> {
    static
    priority_t
    map(cocaine::logging::priorities level);
};

} // namespace sink

BLACKHOLE_END_NS

namespace cocaine { namespace logging {

void
map_severity(blackhole::aux::attachable_ostringstream& stream, const logging::priorities& level);

}} // namespace cocaine::logging

#endif
