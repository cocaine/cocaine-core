/*
    Copyright (c) 2016 Anton Matveenko <antmat@me.com>
    Copyright (c) 2016 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/trace/logger.hpp"

using namespace blackhole;

namespace cocaine { namespace logging {

namespace {
    attribute_list
    gen_view(const attributes_t& attributes) {
        attribute_list attr_list;
        for (const auto& attribute : attributes) {
            attr_list.emplace_back(attribute);
        }
        return attr_list;
    }
}


trace_wrapper_t::trace_wrapper_t(logger_t& log):
    inner(log)
{
}

auto trace_wrapper_t::log(severity_t severity, const message_t& message) -> void {
    auto attr = attributes();
    auto attr_list = gen_view(attr);
    attribute_pack pack{attr_list};
    inner.log(severity, message, pack);
}

auto trace_wrapper_t::log(severity_t severity, const message_t& message, attribute_pack& pack) -> void {
    auto attr = attributes();
    auto attr_list = gen_view(attr);
    pack.push_back(attr_list);
    inner.log(severity, message, pack);
}

auto trace_wrapper_t::log(severity_t severity, const lazy_message_t& message, attribute_pack& pack) -> void {
    auto attr = attributes();
    auto attr_list = gen_view(attr);
    pack.push_back(attr_list);
    inner.log(severity, message, pack);
}

auto trace_wrapper_t::manager() -> scope::manager_t& {
    return inner.manager();
}

}} // namespace cocaine::logging
