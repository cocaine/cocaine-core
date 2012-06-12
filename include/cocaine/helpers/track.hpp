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

#ifndef COCAINE_HELPERS_TRACK_HPP
#define COCAINE_HELPERS_TRACK_HPP

#include <algorithm>

namespace cocaine { namespace helpers {

template<typename T, void (*D)(T)> 
struct track_t {
    public:
        track_t(T object):
            m_object(object)
        { }

        ~track_t() {
            destroy();
        }

        track_t<T, D>& operator=(T object) {
            destroy();
            m_object = object;
            return *this;
        }

        track_t<T, D>& operator=(track_t<T, D>& other) {
            destroy();
            m_object = other.release();
            return *this;
        } 

        T operator*() {
            return m_object;
        }

        const T operator*() const {
            return m_object;
        }

        T* operator&() {
            return &m_object;
        }

        const T* operator&() const {
            return &m_object;
        }

        T operator->() {
            return m_object;
        }

        const T operator->() const {
            return m_object;
        }

        operator T() {
            return m_object;
        }

        operator const T() const {
            return m_object;
        }

        bool valid() const {
            return (m_object != NULL);
        }

        T release() {
            T tmp = NULL;
            std::swap(tmp, m_object);
            return tmp;
        }

        void reset() {
            destroy();
        }

    private:
        void destroy() {
            if(m_object) {
                D(m_object);
            }
        }

        T m_object;
};

}

using helpers::track_t;

}

#endif
