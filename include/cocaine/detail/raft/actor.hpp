/*
    Copyright (c) 2013-2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_RAFT_ACTOR_HPP
#define COCAINE_RAFT_ACTOR_HPP

#include "cocaine/common.hpp"
#include "cocaine/format.hpp"
#include "cocaine/raft/remote.hpp"
#include "cocaine/raft/log.hpp"

namespace cocaine { namespace raft {

enum class state_t {
    leader,
    candidate,
    follower
};

struct actor_concept_t {
    virtual
    std::tuple<uint64_t, bool>
    append(uint64_t term,
           io::raft::node_id_t leader,
           std:tuple<uint64_t, uint64_t> prev_entry, // index, term
           const std::vector<msgpack::object>& entries,
           uint64_t commit_index) = 0;

    virtual
    std::tuple<uint64_t, bool>
    request_vote(uint64_t term,
                 io::raft::node_id_t candidate,
                 std:tuple<uint64_t, uint64_t> last_entry) = 0;
};

template<class Dispatch, class Log = log<Dispatch>>
class actor:
    public actor_concept_t
{
    typedef remote_node<raft_actor<Dispatch, Log>> remote_type;

public:
    typedef Dispatch machine_type;

    typedef Log log_type;

public:
    actor(raft_service_t& service,
          const std::shared_ptr<machine_type>& state_machine):
        m_service(service),
        m_state(state_t::follower),
        m_election_timer(service.reactor()),
        m_applier(service.reactor())
    {
        for(auto it = m_service.cluster().begin(); it != m_service.cluster().end(); ++it) {
            m_cluster.emplace_back(*this, *it);
        }

        m_election_timer.set<actor, &actor::on_disown>(this);
        m_applier.set<actor, &actor::apply_entries>(this);
    }

    ~actor() {
        reset_election_state();
        finish_leadership();
        stop_election_timer();

        if(m_applier.started()) {
            m_applier.stop();
        }
    }

    void
    run() {
        restart_election_timer();
    }

    uint64_t
    current_term() const {
        return m_current_term;
    }

    const log_type&
    log() const {
        return m_log;
    }

    bool
    is_leader() const {
        return m_state == state_t::leader;
    }

    node_id_t
    leader_id() const {
        return m_leader;
    }

    // Send command to the replicated state machine. The actor must be a leader.
    template<class Handler, class... Args>
    bool
    call(Handler&& h, Args&&... args) {
        if(!is_leader()) {
            return false;
        }

        m_log.append(current_term(), std::forward<Args>(args)...);
        m_log.back().bind(std::forward<Handler>(h));

        for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
            try {
                it->replicate();
            } catch(...) {
                // Ignore.
            }
        }
    }

private:
    std::tuple<uint64_t, bool>
    append(uint64_t term,
           io::raft::node_id_t leader,
           std:tuple<uint64_t, uint64_t> prev_entry, // index, term
           const std::vector<msgpack::object>& entries,
           uint64_t commit_index)
    {
        if(term < m_current_term) {
            return std::make_tuple(m_current_term, false);
        }

        step_down(term);

        m_leader = leader;

        if(std::get<0>(prev_entry) > m_log.last_index()) {
            return std::make_tuple(m_current_term, false);
        }

        if(std::get<1>(prev_entry) != m_log.at(std::get<0>(prev_entry)).term()) {
            return std::make_tuple(m_current_term, false);
        }

        uint64_t entry_index = std::get<0>(prev_entry);
        for(auto it = entries.begin(); it != entries.end(); ++it, ++entry_index) {
            log_type::value_type entry;
            io::type_traits<log_type::value_type>::unpack(*it, entry);
            if(m_log.last_index() > entry_index && entry.term() != m_log.at(entry_index).term()) {
                m_log.truncate_suffix(entry_index);
            }

            if(entry_index >= m_log.last_index()) {
                m_log.append(*it);
            }
        }

        m_commit_index = commit_index;

        if(m_commit_index > m_last_applied && !m_applier.started()) {
            m_applier.start();
        }

        return std::make_tuple(m_current_term, true);
    }

    std::tuple<uint64_t, bool>
    request_vote(uint64_t term,
                 io::raft::node_id_t candidate,
                 std:tuple<uint64_t, uint64_t> last_entry)
    {
        if(term > m_current_term) {
            step_down(term);
        }

        if(term == m_current_term &&
           !m_voted_for &&
           (std::get<1>(last_entry) > m_log.last_term() ||
            std::get<1>(last_entry) == m_log.last_term() &&
            std::get<0>(last_entry) >= m_log.last_index()))
        {
            step_down(term);
            m_voted_for = candidate;
        }

        return std::make_tuple(m_current_term, term == m_current_term && m_voted_for == candidate);
    }

    uint64_t
    commit_index() const {
        if(!is_leader() || m_log.last_term() == current_term()) {
            return m_commit_index;
        } else {
            return 0;
        }
    }

    void
    step_down(uint64_t term) {
        if(term > m_current_term) {
            m_current_term = term;
            voted_for.reset();
        }

        restart_election_timer();

        // Disable all non-follower activity.
        reset_election_state();
        finish_leadership();

        m_state = state_t::follower;
    }

    void
    on_disown(ev::timer&, int) {
        start_election();
    }

    void
    apply_entries(ev::idle&, int) {
        if(commit_index() <= m_last_applied) {
            m_applier.stop();
            return;
        }

        for(size_t entry = m_last_applied + 1; entry <= m_commit_index; ++entry) {
            if(m_log.at(entry).is_command()) {
                m_state_machine(m_log.at(entry).get_command());
            }

            ++m_last_applied;
        }
    }

    void
    stop_election_timer() {
        if(!m_election_timer.started()) {
            m_election_timer.stop();
        }
    }

    void
    restart_election_timer() {
        stop_election_timer();
        m_election_timer.start(float(election_timeout + rand() % election_timeout) / 1000.0);
    }

    class election_state_t {
    public:
        election_state_t(actor &actor):
            m_active(true),
            m_votes(0),
            m_actor(actor)
        {
            // Empty.
        }

        void
        disable() {
            m_active = false;
        }

        void
        vote_handler(boost::optional<std::tuple<uint64_t, bool>> result) {
            if(!m_active) {
                return;
            }

            if(result) {
                if(std::get<1>(*result)) {
                    m_granted++;

                    if(m_granted > m_actor.cluster_size() / 2) {
                        m_active = false;
                        m_actor.switch_to_leader();
                    }
                } else if(std::get<0>(*result) > m_actor.term()) {
                    m_active = false;
                    m_actor.step_down(std::get<0>(*result));
                }
            }
        }

    private:
        bool m_active;
        unsigned int m_granted;
        actor &m_actor;
    };

    void
    start_election() {
        using namespace std::placeholders;

        restart_election_timer();
        reset_election_state();

        m_election_state = std::make_shared<election_state_t>(*this);

        m_state = state_t::candidate;

        for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
            try {
                it->second.request_vote(std::bind(&election_state_t::vote_handler, m_election_state, _1),
                                        m_current_term,
                                        m_id,
                                        std::make_tuple(m_log.last_index(), m_log.last_term()));
            } catch(const std::exception&) {
                // Ignore.
            }
        }
    }

    void
    reset_election_state() {
        if(m_election_state) {
            m_election_state->disable();
            m_election_state.reset();
        }
    }

    void
    switch_to_leader() {
        stop_election_timer();
        reset_election_state();

        m_state = state_t::leader;

        call(std::function<void(boost::optional<uint64_t>)>());

        for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
            it->begin_leadership();
        }
    }

    void
    finish_leadership() {
        for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
            it->finish_leadership();
        }

        if(is_leader()) {
            for(auto it = m_log.iter(m_commit_index); it != m_log.end(); ++it) {
                it->notify(boost::none);
            }
        }
    }

    static
    bool
    compare_match_index(const remote_type& left, const remote_type& right) const {
        return left.match_index() < right.match_index();
    }

    void
    update_commit_index() {
        size_t pivot = m_cluster.size() % 2 == 0 ? m_cluster.size() / 2 : m_cluster.size() / 2 + 1;
        std::nth_element(m_cluster.begin(),
                         m_cluster.begin() + pivot,
                         m_cluster.end(),
                         &actor::compare_match_index);
        if(m_cluster[pivot].match_index() > m_commit_index()) {
            auto old_commit_index = m_commit_index;
            m_commit_index = m_cluster[pivot].match_index();
            for(uint64_t i = old_commit_index; i < m_commit_index; ++i) {
                m_log.at(i).notify(i);
            }

            if(m_applier.started()) {
                m_applier.start();
            }
        }
    }

private:
    raft_service_t &m_service;

    std::vector<remote_type> m_cluster;

    state_t m_state;

    std::shared_ptr<machine_type> m_state_machine;

    log_type m_log;

    uint64_t m_current_term;

    // The highest index known to be commited.
    uint64_t m_commit_index;

    // The last entry applied to the state machine.
    uint64_t m_last_applied;

    boost::option<node_id_t> m_voted_for;

    // The leader from the last append message.
    node_id_t m_leader;

    // When the timer fires, the follower will switch to the candidate state.
    ev::timer m_election_timer;

    // When the timer fires, the follower will switch to the candidate state.
    ev::timer m_heartbeat_timer;

    // This watcher will apply committed entries in background.
    ev::idle m_applier;

    // This state will contain information about received votes and will be reset at finish of the election.
    std::shared_ptr<election_state_t> m_election_state;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_ACTOR_HPP
