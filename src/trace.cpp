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

#include "cocaine/errors.hpp"

#include "cocaine/trace/trace.hpp"

#include <boost/thread/tss.hpp>

#include <random>

namespace cocaine {
    trace_t::trace_t() :
        trace_id(zero_value),
        span_id(zero_value),
        parent_id(zero_value),
        grand_parent_id(uninitialized_value),
        rpc_name(),
        service_name()
    {}

    trace_t::trace_t(uint64_t _trace_id,
            uint64_t _span_id,
            uint64_t _parent_id,
            const stack_string_t& _rpc_name,
            const stack_string_t& _service_name) :
        trace_id(_trace_id),
        span_id(_span_id),
        parent_id(_parent_id),
        grand_parent_id(uninitialized_value),
        rpc_name(_rpc_name),
        service_name(_service_name)
    {
        if(parent_id == uninitialized_value ||
           span_id == uninitialized_value ||
           trace_id == uninitialized_value ||
           // Partially empty trace - trace_id without span_id, vise versa or parent_id without trace_id
           ((span_id == zero_value) != (trace_id == zero_value)) ||
           ((trace_id == zero_value) && (parent_id != zero_value))
        ) {
            throw cocaine::error_t("Invalid trace parameters: %llu %llu %llu", trace_id, span_id, parent_id);
        }
    }

    trace_t
    trace_t::generate(const stack_string_t& _rpc_name, const stack_string_t& _service_name) {
        auto t_id = generate_id();
        return trace_t(t_id, t_id, zero_value, _rpc_name, _service_name);
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
        return parent_id;
    }

    uint64_t
    trace_t::get_trace_id() const {
        return trace_id;
    }

    uint64_t
    trace_t::get_id() const {
        return span_id;
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
        BOOST_ASSERT_MSG(parent_id != zero_value, "Can not pop trace - parent_id is 0");
        BOOST_ASSERT_MSG(grand_parent_id != uninitialized_value, "Can not pop trace - grand_parent_id is uninitialized");
        span_id = parent_id;
        parent_id = grand_parent_id;
        rpc_name = parent_rpc_name;
        parent_rpc_name.reset();
        grand_parent_id = uninitialized_value;
    }

    void
    trace_t::push(const stack_string_t& new_rpc_name) {
        if(empty()) {
            return;
        }
        parent_rpc_name = rpc_name;
        rpc_name = new_rpc_name;
        grand_parent_id = parent_id;
        parent_id = span_id;
        span_id = generate_id();
    }

    bool
    trace_t::pushed() const {
        return grand_parent_id != uninitialized_value;
    }

    uint64_t
    trace_t::generate_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        // Stupid zipkin-web can not handle unsigned ids. So we limit to signed diapason.
        static std::uniform_int_distribution<uint64_t> dis(1, std::numeric_limits<uint64_t>::max()/2-1);
        return dis(gen);
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

    trace_t::push_scope_t::push_scope_t(const stack_string_t& _rpc_name){
        if(!trace_t::current().empty()) {
            trace_t::current().push(_rpc_name);
        }
    }

    trace_t::push_scope_t::~push_scope_t() {
        if(!trace_t::current().empty()) {
            trace_t::current().pop();
        }
    }
}

