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
#include "cocaine/common.hpp"

#include <boost/assert.hpp>

#include <deque>

namespace cocaine { namespace raft {

// This class stores log of state machine. Actually instance of this class is part of Raft configuration.
// User can write his own implementation of this class and provide it to the algorithm through configuration (for example to store the log in persistent storage).
// All methods of the log are called only with correct arguments and only when it has sense (i.e. snapshot() is called only after set_snapshot()).
template<class StateMachine>
class log {
    COCAINE_DECLARE_NONCOPYABLE(log)

    typedef std::deque<log_entry<StateMachine>> container_type;

public:
    typedef log_entry<StateMachine> entry_type;
    typedef typename StateMachine::snapshot_type snapshot_type;

    log():
        m_first_index(0)
    { }

    log(log&& other):
        m_first_index(other.m_first_index),
        m_entries(std::move(other.m_entries)),
        m_snapshot(std::move(other.m_snapshot))
    { }

    log&
    operator=(log&& other) {
        m_first_index = other.m_first_index;
        m_entries = std::move(other.m_entries);
        m_snapshot = std::move(other.m_snapshot);
        return *this;
    }

    bool
    empty() const {
        return m_entries.empty() && m_first_index == 0;
    }

    // Access entry by index. First index is 0.
    entry_type&
    operator[](uint64_t index) {
        BOOST_ASSERT(index >= m_first_index);
        return m_entries[index - m_first_index];
    }

    const entry_type&
    operator[](uint64_t index) const {
        BOOST_ASSERT(index >= m_first_index);
        return m_entries[index - m_first_index];
    }

    // Index of the last entry.
    uint64_t
    last_index() const {
        BOOST_ASSERT(m_entries.size() > 0 || m_snapshot);
        return m_first_index + m_entries.size() - 1;
    }

    // Term of the last entry.
    uint64_t
    last_term() const {
        BOOST_ASSERT(m_entries.size() > 0 || m_snapshot);
        return (m_entries.size() > 0) ? m_entries.back().term() : m_snapshot_term;
    }

    // Append an entry to the log.
    template<class... Args>
    void
    push(Args&&... args) {
        m_entries.emplace_back(std::forward<Args>(args)...);
    }

    // Remove tail of the log including entry with given index.
    void
    truncate(uint64_t index) {
        BOOST_ASSERT(index >= m_first_index);
        m_entries.resize(std::min(m_entries.size(), index - m_first_index));
    }

    // Store snapshot of state machine in the log.
    // Last entry applied to the state machine before taking the snapshot has given index and term.
    // It's safe to remove all entries older of the snapshot from the log, because they will not be used.
    // This method must clean the log and setup given snapshot, when the snapshot is newer then last entry or older of current snapshot.
    template<class T>
    void
    set_snapshot(uint64_t index, uint64_t term, T&& snapshot) {
        m_snapshot.reset(new snapshot_type(std::forward<T>(snapshot)));

        if(index >= last_index() || index < m_first_index) {
            m_entries.clear();
        } else {
            m_entries.erase(m_entries.begin(), m_entries.begin() + (index - m_first_index + 1));
        }
        m_first_index = index + 1;
        m_snapshot_term = term;
    }

    // Return index of current snapshot.
    uint64_t
    snapshot_index() const {
        BOOST_ASSERT(m_first_index > 0);
        return m_first_index - 1;
    }

    // Return term of current snapshot.
    uint64_t
    snapshot_term() const {
        BOOST_ASSERT(m_first_index > 0);
        return m_snapshot_term;
    }

    // Return current snapshot.
    const snapshot_type&
    snapshot() const {
        BOOST_ASSERT(m_snapshot.get());
        return *m_snapshot;
    }

private:
    // Index of first entry in m_entries.
    uint64_t m_first_index;

    container_type m_entries;

    uint64_t m_snapshot_term;

    std::unique_ptr<snapshot_type> m_snapshot;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_LOG_HPP
