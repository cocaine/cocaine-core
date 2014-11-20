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

#include "cocaine/detail/bootstrap/logging.hpp"

#include <cxxabi.h>

namespace blackhole { namespace repository { namespace config {

// Converter adapter specializations for dynamic value

dynamic_t
transformer_t<cocaine::dynamic_t>::transform(const value_type& value) {
    if(value.is_null()) {
        throw blackhole::error_t("null values are not supported");
    } else if(value.is_bool()) {
        return value.as_bool();
    } else if(value.is_int()) {
        return value.as_int();
    } else if(value.is_uint()) {
        return value.as_uint();
    } else if(value.is_double()) {
        return value.as_double();
    } else if(value.is_string()) {
        return value.as_string();
    } else if(value.is_array()) {
        dynamic_t::array_t array;
        for(auto it = value.as_array().begin(); it != value.as_array().end(); ++it) {
            array.push_back(transformer_t<value_type>::transform(*it));
        }
        return array;
    } else if(value.is_object()) {
        dynamic_t::object_t object;
        for(auto it = value.as_object().begin(); it != value.as_object().end(); ++it) {
            object[it->first] = transformer_t<value_type>::transform(it->second);
        }
        return object;
    } else {
        BOOST_ASSERT(false);
    }

    return dynamic_t();
}

}}} // namespace blackhole::repository::config

namespace blackhole { namespace sink {

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

}} // namespace blackhole::sink

namespace cocaine { namespace logging {

std::string
demangle(const std::string& mangled) {
    auto custom_deleter = std::bind(&::free, std::placeholders::_1);
    auto status = 0;

    std::unique_ptr<char[], decltype(custom_deleter)> buffer(
        abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status),
        custom_deleter
    );

    static const std::map<int, std::string> errors = {
        {  0, "The demangling operation succeeded." },
        { -1, "A memory allocation failure occurred." },
        { -2, "The mangled name is not a valid name under the C++ ABI mangling rules." },
        { -3, "One of the arguments is invalid." }
    };

    BOOST_ASSERT(errors.count(status));

    if(status != 0) {
        return cocaine::format("failed to demangle '%s': %d", mangled, errors.at(status));
    }

    return buffer.get();
}

// Severity attribute converter from enumeration underlying type into string

void
map_severity(blackhole::aux::attachable_ostringstream& stream, const logging::priorities& level) {
    typedef blackhole::aux::underlying_type<logging::priorities>::type underlying_type;

    static const char* describe[] = {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR"
    };

    const auto value = static_cast<underlying_type>(level);

    if(value < static_cast<underlying_type>(sizeof(describe) / sizeof(describe[0])) && value >= 0) {
        stream << describe[value];
    } else {
        stream << value;
    }
}

}} // namespace cocaine::logging
