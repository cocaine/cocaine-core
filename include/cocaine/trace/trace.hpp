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

#ifndef COCAINE_TRACE_TRACE_HPP
#define COCAINE_TRACE_TRACE_HPP

#include "cocaine/errors.hpp"

#include "cocaine/trace/stack_string.hpp"

#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <boost/thread/tss.hpp>

#include <random>

namespace cocaine {
class trace_t {
public:
    template <class F>
    class callable_wrapper_t;

    class restore_scope_t;
    class push_scope_t;

    // Special value that indicates that field(grand_parent_id) was not set via push.
    static constexpr uint64_t uninitialized_value = -1;
    static constexpr uint64_t zero_value = 0;

    trace_t() :
        trace_id(zero_value),
        span_id(zero_value),
        parent_id(zero_value),
        grand_parent_id(uninitialized_value),
        start_time_us(),
        last_time_us(),
        rpc_name(),
        service_name()
    {}

    template<class ServiceStr, class RpcStr>
    trace_t(uint64_t _trace_id,
            uint64_t _span_id,
            uint64_t _parent_id,
            const RpcStr& _rpc_name,
            const ServiceStr& _service_name) :
        trace_id(_trace_id),
        span_id(_span_id),
        parent_id(_parent_id),
        grand_parent_id(uninitialized_value),
        start_time_us(),
        last_time_us(),
        rpc_name(_rpc_name),
        service_name(_service_name)
    {
        if(parent_id == uninitialized_value ||
           span_id == uninitialized_value ||
           trace_id == uninitialized_value ||
           // Partially empty trace - trace_id without span_id, vise versa or parent_id without trace_id
           (span_id == zero_value != trace_id == zero_value) ||
           (trace_id == zero_value && parent_id != zero_value)
        ) {
            throw cocaine::error_t("Invalid trace parameters: %llu %llu %llu", trace_id, span_id, parent_id);
        }
    }

    template<class ServiceStr, class RpcStr>
    static
    trace_t
    generate(const RpcStr& _rpc_name, const ServiceStr& _service_name) {
        auto t_id = generate_id();
        return trace_t(t_id, t_id, zero_value, _rpc_name, _service_name);
    }

    static
    trace_t&
    current() {
        static boost::thread_specific_ptr<trace_t> t;
        if(t.get() == nullptr) {
            t.reset(new trace_t());
        }
        return *t.get();
    }

    uint64_t
    get_parent_id() const {
        return parent_id;
    }

    uint64_t
    get_trace_id() const {
        return trace_id;
    }

    uint64_t
    get_id() const {
        return span_id;
    }

    bool
    empty() const {
        return trace_id == zero_value;
    }

    void
    pop() {
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

    template<class RpcString>
    void
    push(const RpcString& new_rpc_name) {
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
    pushed() const {
        return grand_parent_id != uninitialized_value;
    }

    template<class AttributeSet>
    AttributeSet
    attributes() const {
        return {
            {"trace_id", {trace_id}},
            {"span_id", {span_id}},
            {"parent_id", {parent_id}},
            {"rpc_name", {rpc_name.blob}},
            {"service_name", {service_name.blob}}
        };
    }

    template<class... Args>
    static
    auto
    bind(Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>;

    template<class Method>
    static
    auto
    mem_fn(Method m) -> callable_wrapper_t<decltype(std::mem_fn(std::forward<Method>(m)))>;

private:
    static
    uint64_t
    generate_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        // Stupid zipkin-web can not handle unsigned ids. So we limit to signed diapason.
        static std::uniform_int_distribution<uint64_t> dis(1, std::numeric_limits<uint64_t>::max()/2-1);
        return dis(gen);
    }

    uint64_t trace_id;
    uint64_t span_id;
    uint64_t parent_id;
    uint64_t grand_parent_id;
    uint64_t start_time_us;
    uint64_t last_time_us;
    stack_str_t<16> parent_rpc_name;
    stack_str_t<16> rpc_name;
    stack_str_t<16> service_name;
};
}

#include "cocaine/trace/trace_impl.hpp"

#endif // COCAINE_TRACE_TRACE_HPP

