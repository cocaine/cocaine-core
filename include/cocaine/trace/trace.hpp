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
    trace_t(uint64_t id, uint64_t span, uint64_t parent, const std::string& name);
    trace_t(uint64_t id, uint64_t span, uint64_t parent, bool verbose, const std::string& name);

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

    /// Returns true if a tracebit was set, indicating that more verbose output is required.
    bool
    verbose() const;

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

    template<typename F>
    static
    auto
    bind(F fn) -> callable_wrapper<F>;

    template<typename F, typename... Args>
    static
    auto
    bind(F fn, Args&& ...args)
        -> callable_wrapper<decltype(std::bind(std::move(fn), std::forward<Args>(args)...))>;

private:
    static
    uint64_t
    generate_id();

    static
    std::string
    to_hex_string(uint64_t val);

    bool m_verbose;
    uint64_t trace_id;
    state_t state;
    state_t previous_state;
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

template<typename F>
class trace_t::callable_wrapper {
    F f;
    trace_t trace;

public:
    callable_wrapper(F f):
        f(std::move(f)),
        trace(trace_t::current())
    {}

    template<typename... Args>
    auto
    operator()(Args&&... args) -> decltype(f(std::forward<Args>(args)...)) {
        restore_scope_t scope(trace);
        return f(std::forward<Args>(args)...);
    }

    template<typename... Args>
    auto
    operator()(Args&&... args) const -> decltype(f(std::forward<Args>(args)...)) {
        restore_scope_t scope(trace);
        return f(std::forward<Args>(args)...);
    }
};

template<typename F>
auto
trace_t::bind(F fn) -> callable_wrapper<F> {
    return {std::move(fn)};
}

template<typename F, typename... Args>
auto
trace_t::bind(F fn, Args&& ...args) -> callable_wrapper<decltype(std::bind(std::move(fn), std::forward<Args>(args)...))> {
    return {std::bind(std::move(fn), std::forward<Args>(args)...)};
}

} // namespace cocaine

#endif // COCAINE_TRACE_TRACE_HPP
