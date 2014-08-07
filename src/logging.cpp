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

#include "cocaine/logging/setup.hpp"

using namespace blackhole::repository::config;
using namespace blackhole::repository::config::adapter;
using namespace blackhole::sink;

using namespace cocaine;

void
logging::map_severity(blackhole::aux::attachable_ostringstream& stream, const logging::priorities& level) {
    static const char* describe[] = {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR"
    };

    typedef blackhole::aux::underlying_type<logging::priorities>::type level_type;

    auto value = static_cast<level_type>(level);

    if(value < static_cast<level_type>(sizeof(describe) / sizeof(describe[0])) && value >= 0) {
        stream << describe[value];
    } else {
        stream << value;
    }
}

array_traits<dynamic_t>::const_iterator
array_traits<dynamic_t>::begin(const value_type& value) {
    BOOST_ASSERT(value.is_array());
    return value.as_array().begin();
}

array_traits<dynamic_t>::const_iterator
array_traits<dynamic_t>::end(const value_type& value) {
    BOOST_ASSERT(value.is_array());
    return value.as_array().end();
}

object_traits<dynamic_t>::const_iterator
object_traits<dynamic_t>::begin(const value_type& value) {
    BOOST_ASSERT(value.is_object());
    return value.as_object().begin();
}

object_traits<dynamic_t>::const_iterator
object_traits<dynamic_t>::end(const value_type& value) {
    BOOST_ASSERT(value.is_object());
    return value.as_object().end();
}

std::string
object_traits<dynamic_t>::name(const const_iterator& it) {
    return std::string(it->first);
}

bool
object_traits<dynamic_t>::has(const value_type& value, const std::string& name) {
    BOOST_ASSERT(value.is_object());
    auto object = value.as_object();
    return object.find(name) != object.end();
}

const object_traits<dynamic_t>::value_type&
object_traits<dynamic_t>::at(const value_type& value, const std::string& name) {
    BOOST_ASSERT(has(value, name));
    return value.as_object()[name];
}

const object_traits<dynamic_t>::value_type&
object_traits<dynamic_t>::value(const const_iterator& it) {
    return it->second;
}

std::string
object_traits<dynamic_t>::as_string(const value_type& value) {
    BOOST_ASSERT(value.is_string());
    return value.as_string();
}

priority_t
priority_traits<logging::priorities>::map(logging::priorities level) {
    switch(level) {
    case logging::debug:
        return priority_t::debug;
    case logging::info:
        return priority_t::info;
    case logging::warning:
        return priority_t::warning;
    case logging::error:
        return priority_t::err;
    default:
        return priority_t::debug;
    }

    return priority_t::debug;
}
