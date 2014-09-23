/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_LOCKED_PTR_HPP
#define COCAINE_LOCKED_PTR_HPP

#include "cocaine/utility.hpp"

#include <mutex>

namespace cocaine {

template<class T, class Lockable = std::mutex>
struct locked_ptr {
    typedef T value_type;
    typedef Lockable mutex_type;

    locked_ptr(value_type& value_, mutex_type& mutex_): value(value_), guard(mutex_) { }
    locked_ptr(locked_ptr&& o): value(o.value), guard(std::move(o.guard)) { }

    T*
    operator->() {
        return &value;
    }

    T&
    operator*() {
        return value;
    }

private:
    value_type& value;
    std::unique_lock<mutex_type> guard;
};

template<class T, class Lockable>
struct locked_ptr<const T, Lockable> {
    typedef T value_type;
    typedef Lockable mutex_type;

    locked_ptr(const value_type& value_, mutex_type& mutex_): value(value_), guard(mutex_) { }
    locked_ptr(locked_ptr&& o): value(o.value), guard(std::move(o.guard)) { }

    const T*
    operator->() const {
        return &value;
    }

    const T&
    operator*() const {
        return value;
    }

private:
    const value_type& value;
    std::unique_lock<mutex_type> guard;
};

template<class T, class Lockable = std::mutex>
struct synchronized {
    typedef T value_type;
    typedef Lockable mutex_type;

    synchronized(): m_value() { }

    // Implicit construction

    synchronized(const value_type& value): m_value(value) { }
    synchronized(value_type&& value): m_value(std::move(value)) { }

    // Forwarding construction

    template<typename... Args>
    synchronized(Args&&... args): m_value(std::forward<Args>(args)...) { }

    // Safe getters

    typedef locked_ptr<T, Lockable> ptr_type;
    typedef locked_ptr<const T, Lockable> const_ptr_type;

    auto
    synchronize() -> ptr_type {
        return ptr_type(m_value, m_mutex);
    }

    auto
    operator->() -> ptr_type {
        return synchronize();
    }

    auto
    synchronize() const -> const_ptr_type {
        return const_ptr_type(m_value, m_mutex);
    }

    auto
    operator->() const -> const_ptr_type {
        return synchronize();
    }

    // Unsafe getters

    auto
    unsafe() -> value_type& {
        return m_value;
    }

    auto
    unsafe() const -> const value_type& {
        return m_value;
    }

    // Synchronized operations

    template<class F>
    auto
    apply(const F& functor) -> typename result_of<F>::type {
        return functor(*synchronize());
    }

    template<class F>
    auto
    apply(const F& functor) const -> typename result_of<F>::type {
        return functor(*synchronize());
    }

private:
    value_type m_value;
    mutable mutex_type m_mutex;
};

} // namespace cocaine

#endif
