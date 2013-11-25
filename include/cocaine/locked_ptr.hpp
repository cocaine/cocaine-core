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

    synchronized(): value() { }
    synchronized(const value_type& value_): value(value_) { }
    synchronized(value_type&& value_): value(std::move(value_)) { }

    locked_ptr<T, Lockable> synchronize() {
        return locked_ptr<T, Lockable>(value, mutex);
    }

    locked_ptr<const T, Lockable> synchronize() const {
        return locked_ptr<const T, Lockable>(value, mutex);
    }

    locked_ptr<T, Lockable> operator->() {
        return locked_ptr<T, Lockable>(value, mutex);
    }

    locked_ptr<const T, Lockable> operator->() const {
        return locked_ptr<const T, Lockable>(value, mutex);
    }

    value_type& get() {
        return value;
    }

    const value_type& get() const {
        return value;
    }

private:
    value_type value;
    mutable mutex_type mutex;
};

} // namespace cocaine

#endif
