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

#ifndef COCAINE_IO_BUFFER_SEQUENCE_HPP
#define COCAINE_IO_BUFFER_SEQUENCE_HPP

#include <boost/asio/buffer.hpp>

namespace cocaine { namespace io {

struct buffer_sequence_t {
    typedef boost::asio::const_buffer value_type;
    typedef const value_type* const_iterator;

    static const size_t kMaximumBuffers = 32;

    template<class Iterator>
    buffer_sequence_t(Iterator begin, Iterator end, size_t offset):
        m_size(0)
    {
        if(begin == end) return;

        for(auto it = begin; it != end && m_size != kMaximumBuffers; ++it) {
            m_buffers[m_size++] = *it;
        }

        m_buffers[0] = m_buffers[0] + offset;
    }

    const_iterator
    begin() const {
        return &m_buffers[0];
    }

    const_iterator
    end() const {
        return &m_buffers[m_size];
    }

private:
    value_type m_buffers[kMaximumBuffers];
    size_t m_size;
};

}} // namespace cocaine::io

#endif
