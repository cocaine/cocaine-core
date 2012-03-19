//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
