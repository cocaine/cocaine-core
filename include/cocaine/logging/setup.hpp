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

#ifndef COCAINE_LOGGING_SETUP_HPP
#define COCAINE_LOGGING_SETUP_HPP

#include "cocaine/common.hpp"
#include "cocaine/dynamic/dynamic.hpp"
#include "cocaine/logging.hpp"

// Blackhole support

namespace cocaine { namespace logging {

void
map_severity(blackhole::aux::attachable_ostringstream& stream, const logging::priorities& level);

}} // namespace cocaine::logging

#include <blackhole/repository/config/parser.hpp>

namespace blackhole { namespace repository { namespace config { namespace adapter {

template<class Builder, class Filler>
struct builder_visitor_t: public boost::static_visitor<> {
    const std::string& path;
    const std::string& name;

    Builder& builder;

    builder_visitor_t(const std::string& path, const std::string& name, Builder& builder) :
        path(path),
        name(name),
        builder(builder)
    { }

    void
    operator()(const cocaine::dynamic_t::null_t&) {
        throw blackhole::error_t("both null and array parsing is not supported");
    }

    template<typename T>
    void
    operator()(const T& value) {
        builder[name] = value;
    }

    void
    operator()(const cocaine::dynamic_t::array_t&) {
        throw blackhole::error_t("both null and array parsing is not supported");
    }

    void
    operator()(const cocaine::dynamic_t::object_t& value) {
        auto nested_builder = builder[name];
        Filler::fill(nested_builder, value, path + "/" + name);
    }
};

template<>
struct array_traits<cocaine::dynamic_t> {
    typedef cocaine::dynamic_t value_type;
    typedef value_type::array_t::const_iterator const_iterator;

    static
    const_iterator
    begin(const value_type& value);

    static
    const_iterator
    end(const value_type& value);
};

template<>
struct object_traits<cocaine::dynamic_t> {
    typedef cocaine::dynamic_t value_type;
    typedef value_type::object_t::const_iterator const_iterator;

    static
    const_iterator
    begin(const value_type& value);

    static
    const_iterator
    end(const value_type& value);

    static
    std::string
    name(const const_iterator& it);

    static
    bool
    has(const value_type& value, const std::string& name);

    static
    const value_type&
    at(const value_type& value, const std::string& name);

    static
    const value_type&
    value(const const_iterator& it);

    static
    std::string
    as_string(const value_type& value);
};

} // namespace adapter

template<>
struct filler<cocaine::dynamic_t> {
    typedef adapter::object_traits<cocaine::dynamic_t> object;

    template<class T>
    static
    void
    fill(T& builder, const cocaine::dynamic_t& node, const std::string& path) {
        for(auto it = object::begin(node); it != object::end(node); ++it) {
            const auto& name = object::name(it);
            const auto& value = object::value(it);

            if(name == "type") {
                continue;
            }

            adapter::builder_visitor_t<T, filler<cocaine::dynamic_t>> visitor {
                path, name, builder
            };

            value.apply(visitor);
        }
    }
};

}}} // namespace blackhole::repository::config

#include <blackhole/frontend/syslog.hpp>

namespace blackhole { namespace sink {

template<>
struct priority_traits<cocaine::logging::priorities> {
    static
    priority_t
    map(cocaine::logging::priorities level);
};

}} // namespace blackhole::sink

#endif
