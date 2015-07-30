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


#ifndef COCAINE_TRACE_TRACE_IMPL_HPP
#define COCAINE_TRACE_TRACE_IMPL_HPP

#include "cocaine/trace/trace.hpp"

namespace cocaine {

class trace_t::restore_scope_t {
public:
    restore_scope_t(const boost::optional<trace_t>& new_trace) :
        old_span(trace_t::current()),
        restored(false)
    {
        if(new_trace && !new_trace->empty()) {
            trace_t::current() = new_trace.get();
            restored = true;
        }
    }
    ~restore_scope_t() {
        if(restored) {
            trace_t::current() = old_span;
        }
    }
private:
    trace_t old_span;
    bool restored;
};


class trace_t::push_scope_t {
public:
    template<class RpcStr>
    push_scope_t(const RpcStr& _rpc_name){
        if(!trace_t::current().empty()) {
            trace_t::current().push(_rpc_name);
        }
    }

    ~push_scope_t() {
        if(!trace_t::current().empty()) {
            trace_t::current().pop();
        }
    }
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

} // namespace cocaine
#endif // COCAINE_TRACE_TRACE_IMPL_HPP

