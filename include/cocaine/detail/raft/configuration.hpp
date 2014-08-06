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
#include "cocaine/api/storage.hpp"

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

    configuration(context_t &context,
                  const std::string& name,
                  const cluster_type& cluster,
                  uint64_t term = 0,
                  uint64_t commit_index = 0,
                  uint64_t last_applied = 0):
        m_storage(api::storage(context, "raft")),
        m_name(name),
        m_cluster(cluster),
        m_current_term(term),
        m_last_vote_term(0),
        m_commit_index(commit_index),
        m_last_applied(last_applied)
    {
        try {
            load();
        } catch(const storage_error_t&) {
            stabilize();
        }
    }

    configuration(configuration&& other):
        m_storage(std::move(other.m_storage)),
        m_name(std::move(other.m_name)),
        m_cluster(std::move(other.m_cluster)),
        m_log(std::move(other.m_log)),
        m_current_term(other.m_current_term),
        m_last_vote_term(other.m_last_vote_term),
        m_commit_index(other.m_commit_index),
        m_last_applied(other.m_last_applied)
    { }

    configuration&
    operator=(configuration&& other) {
        m_storage = std::move(other.m_storage);
        m_name = std::move(other.m_name);
        m_cluster = std::move(other.m_cluster);
        m_log = std::move(other.m_log);
        m_current_term = other.m_current_term;
        m_last_vote_term = other.m_last_vote_term;
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
        stabilize();
    }

    bool
    has_vote() const {
        return m_last_vote_term < m_current_term;
    }

    void
    take_vote() {
        m_last_vote_term = m_current_term;
        stabilize();
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
    void
    load() {
        std::tie(m_current_term, m_last_vote_term) = m_storage->get<std::tuple<uint64_t, uint64_t>>(
            m_name,
            "election_state"
        );
    }

    void
    stabilize() {
        m_storage->put<std::tuple<uint64_t, uint64_t>>(
            m_name,
            "election_state",
            std::make_tuple(m_current_term, m_last_vote_term),
            std::vector<std::string>()
        );
    }

private:
    std::shared_ptr<api::storage_t> m_storage;

    // Name of the state machine. It's used as a namespace in the storage.
    std::string m_name;

    // Set of nodes in the RAFT cluster.
    cluster_type m_cluster;

    // Log of commands for the state machine.
    log_type m_log;

    // Current term.
    uint64_t m_current_term;

    // The last term the node voted in. The node may vote only once per term.
    uint64_t m_last_vote_term;

    // The highest index known to be committed.
    uint64_t m_commit_index;

    // The last entry applied to the state machine.
    uint64_t m_last_applied;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_CONFIGURATION_HPP
