/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_RAFT_LOG_HPP
#define COCAINE_RAFT_LOG_HPP

#include "cocaine/detail/raft/forwards.hpp"

#include <vector>

namespace cocaine { namespace raft {

template<class StateMachine>
class log {
public:
    typedef log_entry<StateMachine> entry_type;
    typedef typename std::vector<entry_type>::iterator iterator;
    typedef typename std::vector<entry_type>::const_iterator const_iterator;

    bool
    empty() const {
        return m_data.empty();
    }

    entry_type&
    at(uint64_t index) {
        return m_data[index];
    }

    const entry_type&
    at(uint64_t index) const {
        return m_data[index];
    }

    iterator
    iter(uint64_t index) {
        return m_data.begin() + index;
    }

    const_iterator
    iter(uint64_t index) const {
        return m_data.begin() + index;
    }

    iterator
    end() {
        return m_data.end();
    }

    const_iterator
    end() const {
        return m_data.end();
    }

    uint64_t
    last_index() const {
        return m_data.size() - 1;
    }

    uint64_t
    last_term() const {
        return m_data.back().term();
    }

    template<class... Args>
    void
    append(Args&&... args) {
        m_data.emplace_back(std::forward<Args>(args)...);
    }

    void
    truncate_suffix(uint64_t index) {
        m_data.resize(index);
    }

private:
    std::vector<entry_type> m_data;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_LOG_HPP
