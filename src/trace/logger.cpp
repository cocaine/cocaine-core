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

#include "cocaine/context/filter.hpp"
#include "cocaine/trace/logger.hpp"

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


trace_wrapper_t::trace_wrapper_t(std::unique_ptr<blackhole::logger_t> log):
    inner(std::move(log)),
    m_filter(new filter_t([](severity_t, attribute_pack&) { return true; }))
{
}

auto trace_wrapper_t::attributes() const noexcept -> blackhole::attributes_t {
    if(!trace_t::current().empty()) {
        return trace_t::current().formatted_attributes<blackhole::attributes_t>();
    }

    return blackhole::attributes_t();
}

auto trace_wrapper_t::filter(filter_t new_filter) -> void {
    std::shared_ptr<filter_t> new_filter_ptr(new filter_t(std::move(new_filter)));
    m_filter.apply([&](std::shared_ptr<filter_t>& filter_ptr){
        filter_ptr.swap(new_filter_ptr);
    });
}

auto trace_wrapper_t::log(severity_t severity, const message_t& message) -> void {
    attribute_pack pack;
    log(severity, message, pack);
}

auto trace_wrapper_t::log(severity_t severity, const message_t& message, attribute_pack& pack) -> void {
    auto attr = attributes();
    auto attr_list = gen_view(attr);
    pack.push_back(attr_list);
    auto filter = *(m_filter.synchronize());
    if(filter->operator()(severity, pack)) {
        inner->log(severity, message, pack);
    }
}

auto trace_wrapper_t::log(severity_t severity, const lazy_message_t& message, attribute_pack& pack) -> void {
    auto attr = attributes();
    auto attr_list = gen_view(attr);
    pack.push_back(attr_list);
    auto filter = *(m_filter.synchronize());
    if(filter->operator()(severity, pack)) {
        inner->log(severity, message, pack);
    }
}

auto trace_wrapper_t::manager() -> scope::manager_t& {
    return inner->manager();
}

}} // namespace cocaine::logging
