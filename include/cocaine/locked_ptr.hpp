/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include <mutex>

namespace cocaine {

template<class T, class Lockable = std::mutex>
struct locked_ptr {
    typedef T        value_type;
    typedef Lockable mutex_type;

    locked_ptr(value_type& value_, mutex_type& mutex_): value(value_), guard(mutex_) { }
    locked_ptr(locked_ptr&& o): value(o.value), guard(std::move(o.guard)) { }

    T* operator->() { return &value; }
    T& operator* () { return  value; }

private:
    value_type& value;
    std::unique_lock<mutex_type> guard;
};

template<class T, class Lockable>
struct locked_ptr<const T, Lockable> {
    typedef T        value_type;
    typedef Lockable mutex_type;

    locked_ptr(const value_type& value_, mutex_type& mutex_): value(value_), guard(mutex_) { }
    locked_ptr(locked_ptr&& o): value(o.value), guard(std::move(o.guard)) { }

    const T* operator->() const { return &value; }
    const T& operator* () const { return  value; }

private:
    const value_type& value;
    std::unique_lock<mutex_type> guard;
};

template<class T, class Lockable = std::mutex>
struct synchronized {
    typedef T        value_type;
    typedef Lockable mutex_type;

    synchronized(): m_value() { }
    synchronized(const value_type& value): m_value(value) { }
    synchronized(value_type&& value): m_value(std::move(value)) { }

    auto
    value() -> value_type& {
        return m_value;
    }

    auto
    value() const -> const value_type& {
        return m_value;
    }

    typedef locked_ptr<T, Lockable> ptr_type;
    typedef locked_ptr<const T, Lockable> const_ptr_type;

    auto
    synchronize() -> ptr_type {
        return ptr_type(m_value, m_mutex);
    }

    auto
    synchronize() const -> const_ptr_type {
        return const_ptr_type(m_value, m_mutex);
    }

    auto
    operator->() -> ptr_type {
        return synchronize();
    }

    auto
    operator->() const -> const_ptr_type {
        return synchronize();
    }

private:
    value_type m_value;
    mutable mutex_type m_mutex;
};

} // namespace cocaine

#endif
