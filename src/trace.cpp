/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
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

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/trace/trace.hpp"

#include "cocaine/errors.hpp"

#include <random>
#include <sstream>

#include <boost/thread/tss.hpp>

using namespace cocaine;

trace_t::trace_t():
    trace_id(zero_value),
    state({zero_value, zero_value, {}})
{}

trace_t::trace_t(uint64_t trace_id_,
                 uint64_t span_id_,
                 uint64_t parent_id_,
                 const std::string& rpc_name_):
    trace_id(trace_id_),
    state({span_id_, parent_id_, rpc_name_}),
    previous_state(boost::none)
{
    auto check_range = [](uint64_t value) -> bool {
        static const uint64_t max = (1ull << 63) - 1;
        return value <= max;
    };

    // Check that values are in valid range.
    if(!check_range(trace_id) || !check_range(state.span_id) || !check_range(state.parent_id)) {
        throw error::error_t("invalid trace parameters: %llu %llu %llu", trace_id, state.span_id, state.parent_id);
    }

    if(trace_id == zero_value) {
        // If we create empty trace all values should be zero
        if(state.parent_id != zero_value || state.span_id != zero_value) {
            throw error::error_t("invalid trace parameters: %llu %llu %llu", trace_id, state.span_id, state.parent_id);
        }
    } else {
        // If trace_id is not zero - span_id should be present.
        if(state.span_id == zero_value) {
            throw error::error_t("invalid trace parameters: %llu %llu %llu", trace_id, state.span_id, state.parent_id);
        }
    }
}

trace_t
trace_t::generate(const std::string& rpc_name) {
    auto t_id = generate_id();
    return trace_t(t_id, t_id, zero_value, rpc_name);
}

trace_t&
trace_t::current() {
    static boost::thread_specific_ptr<trace_t> t;
    if(t.get() == nullptr) {
        t.reset(new trace_t());
    }
    return *t.get();
}

uint64_t
trace_t::get_parent_id() const {
    return state.parent_id;
}

uint64_t
trace_t::get_trace_id() const {
    return trace_id;
}

uint64_t
trace_t::get_id() const {
    return state.span_id;
}

bool
trace_t::empty() const {
    return trace_id == zero_value;
}

void
trace_t::pop() {
    if(empty()) {
        return;
    }
    BOOST_ASSERT_MSG(state.parent_id != zero_value, "cannot pop trace - parent_id is 0");
    BOOST_ASSERT_MSG(previous_state.is_initialized(), "cannot pop trace - pushed state is none");
    state = previous_state.get();
    previous_state = boost::none;
}

void
trace_t::push(const std::string& new_rpc_name) {
    if(empty()) {
        return;
    }
    previous_state = state;
    state.span_id = generate_id();
    state.parent_id = previous_state->span_id;
    state.rpc_name = new_rpc_name;
}

bool
trace_t::pushed() const {
    return previous_state.is_initialized();
}

uint64_t
trace_t::generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    // Stupid zipkin-web can not handle unsigned ids. So we limit to signed diapason.
    static std::uniform_int_distribution<uint64_t> dis(1, std::numeric_limits<uint64_t>::max()/2-1);
    return dis(gen);
}

std::string
trace_t::to_hex_string(uint64_t value) {
    std::ostringstream oss;
    oss << std::hex << value;
    return oss.str();
}

trace_t::restore_scope_t::restore_scope_t(const boost::optional<trace_t>& new_trace) :
    old_span(trace_t::current()),
    restored(false)
{
    if(new_trace && !new_trace->empty()) {
        trace_t::current() = new_trace.get();
        restored = true;
    }
}

trace_t::restore_scope_t::~restore_scope_t() {
    if(restored) {
        trace_t::current() = old_span;
    }
}

trace_t::push_scope_t::push_scope_t(const std::string& rpc_name){
    if(!trace_t::current().empty()) {
        trace_t::current().push(rpc_name);
    }
}

trace_t::push_scope_t::~push_scope_t() {
    if(!trace_t::current().empty()) {
        trace_t::current().pop();
    }
}
