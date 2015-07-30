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

#include "cocaine/trace/stack_string.hpp"

#include <boost/thread/tss.hpp>
#include <boost/optional.hpp>

#include <cassert>
#include <random>

namespace cocaine {
class trace_t {
public:
    template <class F>
    class callable_wrapper_t;

    class restore_scope_t;
    class push_scope_t;

    trace_t() :
        trace_id(),
        span_id(),
        parent_id(),
        parent_parent_id(),
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
        parent_parent_id(),
        start_time_us(),
        last_time_us(),
        rpc_name(_rpc_name),
        service_name(_service_name)
    {
    }

    template<class ServiceStr, class RpcStr>
    static
    trace_t
    generate(const RpcStr& _rpc_name, const ServiceStr& _service_name) {
        auto t_id = generate_id();
        return trace_t(t_id, t_id, 0, _rpc_name, _service_name);
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
        return trace_id == 0;
    }

    void
    pop() {
        assert(parent_id != 0);
        span_id = parent_id;
        parent_id = parent_parent_id;
        rpc_name = parent_rpc_name;
        parent_rpc_name.reset();
        parent_parent_id = 0;
    }

    template<class RpcString>
    void
    push(const RpcString& new_rpc_name) {
        parent_rpc_name = rpc_name;
        rpc_name = new_rpc_name;
        parent_parent_id = parent_id;
        parent_id = span_id;
        span_id = generate_id();
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
        static std::uniform_int_distribution<uint64_t> dis(1, std::numeric_limits<uint64_t>::max()/2-1);
        return dis(gen);
    }

    uint64_t trace_id;
    uint64_t span_id;
    uint64_t parent_id;
    uint64_t parent_parent_id;
    uint64_t start_time_us;
    uint64_t last_time_us;
    stack_str_t<16> parent_rpc_name;
    stack_str_t<16> rpc_name;
    stack_str_t<16> service_name;
};
}

#include "cocaine/trace/trace_impl.hpp"

#endif // COCAINE_TRACE_TRACE_HPP

