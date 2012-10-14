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

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 4) && !defined(__clang__)
    #include <cstdatomic>
#else
    #include <atomic>
#endif

namespace cocaine { namespace helpers {

template<class T>
class birth_control  {
    public:
        birth_control() {
            ++g_objects_alive;
            ++g_objects_created;
        }

        static
        uint64_t
        objects_alive() {
            return g_objects_alive;
        }

        static
        uint64_t
        objects_created() {
            return g_objects_created;
        }

    protected:
        ~birth_control() {
            --g_objects_alive;
        }

    private:
        static std::atomic<uint64_t> g_objects_alive;
        static std::atomic<uint64_t> g_objects_created;
};

template<class T>
std::atomic<uint64_t>
birth_control<T>::g_objects_alive(0);

template<class T>
std::atomic<uint64_t>
birth_control<T>::g_objects_created(0);

} // namespace helpers

using helpers::birth_control;

} // namespace cocaine

#endif
