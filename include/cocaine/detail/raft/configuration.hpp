/*
    Copyright (c) 2014-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_RAFT_CONFIGURATION_HPP
#define COCAINE_RAFT_CONFIGURATION_HPP

#include "cocaine/detail/raft/forwards.hpp"
#include "cocaine/detail/raft/log.hpp"

#include <boost/optional.hpp>

#include <set>

namespace cocaine { namespace raft {

// This class stores state of the Raft algorithm when it's running.
// User can write his own implementation of this class and specialize the algorithm with it
// (for example to store the log and the state in persistent storage).
template<class StateMachine>
class configuration {
    COCAINE_DECLARE_NONCOPYABLE(configuration)

public:
    typedef cluster_config_t cluster_type;
    typedef cocaine::raft::log<StateMachine, cluster_type> log_type;

    configuration(const cluster_type& cluster,
                  log_type&& log = log_type(),
                  uint64_t term = 0,
                  uint64_t commit_index = 0,
                  uint64_t last_applied = 0):
        m_cluster(cluster),
        m_log(std::move(log)),
        m_current_term(term),
        m_commit_index(commit_index),
        m_last_applied(last_applied)
    { };

    configuration(configuration&& other):
        m_cluster(std::move(other.m_cluster)),
        m_log(std::move(other.m_log)),
        m_current_term(other.m_current_term),
        m_commit_index(other.m_commit_index),
        m_last_applied(other.m_last_applied)
    { };

    configuration&
    operator=(configuration&& other) {
        m_cluster = std::move(other.m_cluster);
        m_log = std::move(other.m_log);
        m_current_term = other.m_current_term;
        m_commit_index = other.m_commit_index;
        m_last_applied = other.m_last_applied;
        return *this;
    }

    cluster_type&
    cluster() {
        return m_cluster;
    }

    const cluster_type&
    cluster() const {
        return m_cluster;
    }

    log_type&
    log() {
        return m_log;
    }

    const log_type&
    log() const {
        return m_log;
    }

    uint64_t
    current_term() const {
        return m_current_term;
    }

    void
    set_current_term(uint64_t value) {
        m_current_term = value;
    }

    uint64_t
    commit_index() const {
        return m_commit_index;
    }

    void
    set_commit_index(uint64_t value) {
        m_commit_index = value;
    }

    uint64_t
    last_applied() const {
        return m_last_applied;
    }

    void
    set_last_applied(uint64_t value) {
        m_last_applied = value;
    }

private:
    // Set of nodes in the RAFT cluster.
    cluster_type m_cluster;

    // Log of commands for the state machine.
    log_type m_log;

    // Current term.
    uint64_t m_current_term;

    // The highest index known to be committed.
    uint64_t m_commit_index;

    // The last entry applied to the state machine.
    uint64_t m_last_applied;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_CONFIGURATION_HPP
