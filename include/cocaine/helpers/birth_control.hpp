/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_HELPERS_BIRTH_CONTROL_HPP
#define COCAINE_HELPERS_BIRTH_CONTROL_HPP

#include <boost/thread/shared_mutex.hpp>

namespace cocaine { namespace helpers {

template<class T>
class birth_control  {
    public:
        birth_control() {
            boost::unique_lock<boost::shared_mutex> lock(g_mutex);
            ++g_objects_alive;
            ++g_objects_created;
        }

        static
        uint64_t
        objects_alive() {
            boost::shared_lock<boost::shared_mutex> lock(g_mutex);
            return g_objects_alive;
        }

        static
        uint64_t
        objects_created() {
            boost::shared_lock<boost::shared_mutex> lock(g_mutex);
            return g_objects_created;
        }

    protected:
        ~birth_control() {
            boost::unique_lock<boost::shared_mutex> lock(g_mutex);
            --g_objects_alive;
        }

    private:
        static uint64_t g_objects_alive;
        static uint64_t g_objects_created;

        static boost::shared_mutex g_mutex;
};

template<class T>
uint64_t
birth_control<T>::g_objects_alive(0);

template<class T>
uint64_t
birth_control<T>::g_objects_created(0);

template<class T>
boost::shared_mutex
birth_control<T>::g_mutex;

} // namespace helpers

using helpers::birth_control;

} // namespace cocaine

#endif
