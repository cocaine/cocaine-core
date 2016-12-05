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

#include "cocaine/common.hpp"

#include <boost/optional/optional_fwd.hpp>

#include <functional>
#include <string>

namespace cocaine {

class trace_t
{
public:
    template<class F>
    class callable_wrapper;

    class restore_scope_t;
    class push_scope_t;

    struct state_t {
        uint64_t span_id;
        uint64_t parent_id;
        std::string rpc_name;
    };

    static constexpr uint64_t zero_value = 0;

    /**
     * Construct an empty trace.
     */
    trace_t();

    /**
     * Construct trace with specified tuple of ids and service and rpc name.
     */
    trace_t(uint64_t trace_id,
            uint64_t span_id,
            uint64_t parent_id,
            const std::string& rpc_name);

    /**
     * Generate a new trace with specified service and rpc name.
     */
    static
    trace_t
    generate(const std::string& rpc_name);

    /**
     * Return current trace.
     * Trace is usually set via scope guards and passed via callback wrapper in async callbacks.
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
     * Check if trace is empty (was not set via any of scope guards).
     */
    bool
    empty() const;

    /**
     * Pop trace after finished RPC call.
     */
    void
    pop();

    /**
     * Push trace before making an RPC call.
     */
    void
    push(const std::string& new_rpc_name);

    /**
     * Check if trace were pushed.
     */
    bool
    pushed() const;

    template<class AttributeSet>
    AttributeSet
    formatted_attributes() const {
        return {
            {"trace_id", {to_hex_string(trace_id)}},
            {"span_id", {to_hex_string(state.span_id)}},
            {"parent_id", {to_hex_string(state.parent_id)}},
            {"rpc_name", {state.rpc_name}}
        };
    }

    template<class AttributeSet>
    AttributeSet
    attributes() const {
        return {
            {"trace_id", {trace_id}},
            {"span_id", {state.span_id}},
            {"parent_id", {state.parent_id}},
            {"rpc_name", {state.rpc_name}}
        };
    }

    template<class... Args>
    static
    auto
    bind(Args&& ...args) -> callable_wrapper<decltype(std::bind(std::forward<Args>(args)...))>;

    template<class Method>
    static
    auto
    mem_fn(Method m) -> callable_wrapper<decltype(std::mem_fn(std::forward<Method>(m)))>;

private:
    static
    uint64_t
    generate_id();

    static
    std::string
    to_hex_string(uint64_t val);

    uint64_t trace_id;
    state_t state, previous_state;
    bool was_pushed;
};

class trace_t::restore_scope_t
{
public:
    restore_scope_t(const boost::optional<trace_t>& new_trace);
   ~restore_scope_t();

private:
    trace_t old_span;
    bool restored;
};


class trace_t::push_scope_t
{
public:
    push_scope_t(const std::string& rpc_name);
   ~push_scope_t();
};

template <class F>
class trace_t::callable_wrapper
{
public:
    inline
    callable_wrapper(F&& f_):
        f(std::move(f_)),
        stored_trace(trace_t::current())
    {}

    template<class... Args>
    auto
    operator()(Args&&... args) -> decltype(std::declval<F>()(std::forward<Args>(args)...)) {
        restore_scope_t scope(stored_trace);
        return f(std::forward<Args>(args)...);
    }

private:
    F f;
    trace_t stored_trace;
};

template<class... Args>
auto
trace_t::bind(Args&& ...args) -> callable_wrapper<decltype(std::bind(std::forward<Args>(args)...))> {
    typedef callable_wrapper<decltype(std::bind(std::forward<Args>(args)...))> result_type;
    return result_type(std::bind(std::forward<Args>(args)...));
}

template<class Method>
auto
trace_t::mem_fn(Method m) -> callable_wrapper<decltype(std::mem_fn(std::forward<Method>(m)))> {
    typedef callable_wrapper<decltype(std::mem_fn(std::forward<Method>(m)))> result_type;
    return result_type(std::mem_fn(std::forward<Method>(m)));
}

} // namespace cocaine

#endif // COCAINE_TRACE_TRACE_HPP
