/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
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

#ifndef COCAINE_HELPERS_BLOB_HPP
#define COCAINE_HELPERS_BLOB_HPP

#include <boost/detail/atomic_count.hpp>
#include <boost/shared_ptr.hpp>

namespace cocaine { namespace helpers {

struct none;
struct hash;

template<class IntegrityPolicy>
class blob_t {
    public:

    public:
        blob_t():
            m_data(NULL),
            m_size(0)
        { }

        blob_t(const void * data, size_t size):
            m_data(NULL),
            m_size(0)
        {
            if(data == NULL || size == 0) {
                return;
            }

            m_ref_counter.reset(new reference_counter(1));

            m_data = new unsigned char[size];
            m_size = size;

            memcpy(m_data, data, size);
        }
        
        blob_t(const blob_t& other):
            m_data(NULL),
            m_size(0)
        {
            copy(other);
        }

        ~blob_t() {
            clear();
        }
 
        blob_t& operator=(const blob_t& other) {
            if(&other != this) {
                clear();
                copy(other);
            }
            
            return *this;
        }
        
        bool operator==(const blob_t& other) const;
        bool operator!=(const blob_t& other) const;

        const void* data() const {
            return static_cast<const void*>(m_data);
        }

        size_t size() const {
            return m_size;
        }

        bool empty() const {
            return m_data == NULL && m_size == 0;
        }
        
        void clear() {
            if(empty()) {
                return;
            }

            if(--*m_ref_counter == 0) {
                delete[] m_data;
                m_ref_counter.reset();
            }

            m_data = NULL;
            m_size = 0;
        }

    private:
        void copy(const blob_t& other) {
            if(other.empty()) {
                return;
            }

            m_data = other.m_data;
            m_size = other.m_size;
            m_ref_counter = other.m_ref_counter;

            ++*m_ref_counter;
        }
        
    private:
        // Data
        unsigned char * m_data;
        size_t m_size;

        // Atomic reference counter
        typedef boost::detail::atomic_count reference_counter;
        boost::shared_ptr<reference_counter> m_ref_counter;
};

}

typedef helpers::blob_t<helpers::none> blob_t;

}

#endif // COCAINE_HELPERS_BLOB_HPP
