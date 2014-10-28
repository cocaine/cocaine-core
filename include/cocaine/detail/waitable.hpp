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

#ifndef COCAINE_WAITABLE_HPP
#define COCAINE_WAITABLE_HPP

#include "cocaine/common.hpp"

#include <condition_variable>
#include <mutex>

namespace cocaine {

template<class Target>
class waitable {
    std::condition_variable      condition;
    std::mutex                   mutex;
    std::unique_lock<std::mutex> operation;

public:
    waitable():
        operation(mutex)
    { }

    void
    operator()() {
        {
            std::unique_lock<std::mutex> _(mutex);

            // Calls the target operation while holding the monitor's mutex. In fact, prelocking the
            // operation mutex in constructor allows us to skip spinning on the monitor.
            static_cast<Target*>(this)->execute();
        }

        condition.notify_one();
    }

    void
    wait() {
        condition.wait(operation);
    }
};

} // namespace cocaine

#endif
