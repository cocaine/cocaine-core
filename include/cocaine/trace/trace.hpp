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

#include <boost/assert.hpp>
#include <boost/optional.hpp>

namespace cocaine {
class trace_t {
public:
    template <class F>
    class callable_wrapper_t;


    class restore_scope_t;
    class push_scope_t;

    typedef stack_str<16> stack_string_t;
    // Special value that indicates that field(grand_parent_id) was not set via push.
    static constexpr uint64_t uninitialized_value = -1;
    static constexpr uint64_t zero_value = 0;

    /**
     * Construct an empty trace
     */
    trace_t();

    /**
     * Construct trace wih specified tuple of ids and service and rpc name
     */
    trace_t(uint64_t _trace_id,
            uint64_t _span_id,
            uint64_t _parent_id,
            const stack_string_t& _rpc_name,
            const stack_string_t& _service_name);

    /**
     * Generate a new trace with specified service and rpc name
     */
    static
    trace_t
    generate(const stack_string_t& _rpc_name, const stack_string_t& _service_name);

    /**
     * Return current trace.
     * Trace is usually set via scope guards
     * and passed via callback wrapper in async callbacks.
     */
    static
    trace_t&
    current();

    uint64_t
    get_parent_id() const;

    uint64_t
    get_trace_id() const;

    uint64_t
    get_id() const;

    /**
     * Check if trace is empty (was not set via any of scope guards)
     */
    bool
    empty() const;

    /**
     * Pop trace after finished RPC call
     */
    void
    pop();

    /**
     * Push trace before making an RPC call
     */
    void
    push(const stack_string_t& new_rpc_name);

    /**
     * Check if trace were pushed.
     */
    bool
    pushed() const;

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
    generate_id();

    uint64_t trace_id;
    uint64_t span_id;
    uint64_t parent_id;
    uint64_t grand_parent_id;
    stack_string_t parent_rpc_name;
    stack_string_t rpc_name;
    stack_string_t service_name;
};

class trace_t::restore_scope_t {
public:
    restore_scope_t(const boost::optional<trace_t>& new_trace);
    ~restore_scope_t();
private:
    trace_t old_span;
    bool restored;
};


class trace_t::push_scope_t {
public:
    push_scope_t(const stack_string_t& _rpc_name);
    ~push_scope_t();
};

template <class F>
class trace_t::callable_wrapper_t
{
public:
    inline
    callable_wrapper_t(F&& _f) :
        f(std::move(_f)),
        stored_trace(trace_t::current())
    {}

    template<class ...Args>
    auto
    operator()(Args&& ...args) -> decltype(std::declval<F>()(args...)) {
        restore_scope_t scope(stored_trace);
        return f(std::forward<Args>(args)...);
    }

private:
    F f;
    trace_t stored_trace;
};

template<class... Args>
auto
trace_t::bind(Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))> {
    typedef callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))> Result;
    return Result(std::bind(std::forward<Args>(args)...));
}

template<class Method>
auto
trace_t::mem_fn(Method m) -> callable_wrapper_t<decltype(std::mem_fn(std::forward<Method>(m)))> {
    typedef callable_wrapper_t<decltype(std::mem_fn(std::forward<Method>(m)))> Result;
    return Result(std::mem_fn(std::forward<Method>(m)));
}
}

#endif // COCAINE_TRACE_TRACE_HPP

