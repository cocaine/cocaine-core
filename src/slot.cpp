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

#include "cocaine/slot.hpp"

namespace cocaine { namespace detail {

state_t::state_t():
    m_packer(m_buffer),
    m_completed(false),
    m_failed(false)
{ }

void
state_t::abort(error_code code, const std::string& reason) {
    std::unique_lock<std::mutex> lock(m_mutex);

    if(m_completed) {
        return;
    }

    m_code = code;
    m_reason = reason;

    if(m_upstream) {
        m_upstream->error(m_code, m_reason);
        m_upstream->close();
    }

    m_failed = true;
}

void
state_t::close() {
    std::unique_lock<std::mutex> lock(m_mutex);

    if(m_completed) {
        return;
    }

    if(m_upstream) {
        m_upstream->close();
    }

    m_completed = true;
}

void
state_t::attach(const api::stream_ptr_t& upstream) {
    std::unique_lock<std::mutex> lock(m_mutex);

    m_upstream = upstream;

    if(m_completed || m_failed) {
        if(m_completed) {
            m_upstream->write(m_buffer.data(), m_buffer.size());
        } else if(m_failed) {
            m_upstream->error(m_code, m_reason);
        }

        m_upstream->close();
    }
}

}} // namespace cocaine::detail
