/*
    Copyright (c) 2014-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_IO_CANCELABLE_TASK_HPP
#define COCAINE_IO_CANCELABLE_TASK_HPP

#include <functional>
#include <memory>

namespace cocaine { namespace io {

template<class Functor>
class task {
public:
    typedef Functor functor_type;
    typedef void result_type;

    explicit
    task(const std::shared_ptr<functor_type>& callback = std::shared_ptr<functor_type>()):
        m_callback(callback)
    { }

    template<class... Args>
    void
    operator()(Args&&... args) {
        auto callback = m_callback.lock();
        if(callback) {
            (*callback)(std::forward<Args>(args)...);
        }
    }

private:
    std::weak_ptr<functor_type> m_callback;
};

template<class Functor>
task<Functor>
make_task(const std::shared_ptr<Functor>& callback) {
    return task<Functor>(callback);
}

}} // namespace cocaine::io

#endif // COCAINE_IO_CANCELABLE_TASK_HPP
