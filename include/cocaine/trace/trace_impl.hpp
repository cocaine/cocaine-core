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


#ifndef TRACE_IMPL_HPP
#define TRACE_IMPL_HPP

#include "cocaine/trace/trace.hpp"

namespace cocaine {

template<class Logger, class ScopedAttribute, class AttributeSet>
template <class F>
class trace<Logger, ScopedAttribute, AttributeSet>::callable_wrapper_t
{
public:
    typedef trace<Logger, ScopedAttribute, AttributeSet> trace_type;
    inline
    callable_wrapper_t(F&& _f) :
        f(std::move(_f)),
        stored_trace(trace_type::current())
    {}

    template<class ...Args>
    auto
    operator()(Args&& ...args) -> decltype(std::declval<F>()(args...)) {
        restore_scope_t scope(stored_trace);
        ScopedAttribute attr_scope(*trace_type::get_logger(), stored_trace.attributes());
        return f(std::forward<Args>(args)...);
    }

private:
    F f;
    trace_type stored_trace;
};

template<class Logger, class ScopedAttribute, class AttributeSet>
template<class... Args>
auto
trace<Logger, ScopedAttribute, AttributeSet>::bind(Args&& ...args)
-> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>
{
    typedef callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))> Result;
    return Result(std::bind(std::forward<Args>(args)...));
}

template<class Logger, class ScopedAttribute, class AttributeSet>
template<class Method>
auto
trace<Logger, ScopedAttribute, AttributeSet>::mem_fn(Method m)
-> callable_wrapper_t<decltype(std::mem_fn(std::forward<Method>(m)))>
{
    typedef callable_wrapper_t<decltype(std::mem_fn(std::forward<Method>(m)))> Result;
    return Result(std::mem_fn(std::forward<Method>(m)));
}
}
#endif // TRACE_IMPL_HPP

