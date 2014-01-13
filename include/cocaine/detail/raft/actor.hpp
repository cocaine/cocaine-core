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

#include "cocaine/detail/raft/log.hpp"
#include "cocaine/detail/raft/remote.hpp"

#include <memory>
#include <vector>

namespace cocaine { namespace raft {

enum class state_t {
    leader,
    candidate,
    follower
};

class actor_concept_t {
public:
    virtual
    deferred<std::tuple<uint64_t, bool>>
    append(uint64_t term,
           raft::node_id_t leader,
           std::tuple<uint64_t, uint64_t> prev_entry, // index, term
           const std::vector<msgpack::object>& entries,
           uint64_t commit_index) = 0;

    virtual
    deferred<std::tuple<uint64_t, bool>>
    request_vote(uint64_t term,
                 raft::node_id_t candidate,
                 std::tuple<uint64_t, uint64_t> last_entry) = 0;
};

template<class Dispatch, class Log>
class actor:
    public actor_concept_t
{
    typedef remote_node<actor<Dispatch, Log>> remote_type;

    template<class> friend class remote_node;

public:
    typedef Dispatch machine_type;

    typedef Log log_type;

public:
    template<class LogArg>
    actor(service::raft_t& service,
          const std::string& name,
          const std::shared_ptr<machine_type>& state_machine,
          LogArg&& log):
        m_service(service),
        m_logger(new logging::log_t(service.context(), "raft/" + name)),
        m_name(name),
        m_state(state_t::follower),
        m_state_machine(state_machine),
        m_log(std::forward<LogArg>(log)),
        m_current_term(0),
        m_commit_index(0),
        m_last_applied(0),
        m_election_timer(service.reactor().native()),
        m_applier(service.reactor().native())
    {
        COCAINE_LOG_DEBUG(m_logger, "Initialize raft actor with name %s.", name);

        for(auto it = m_service.cluster().begin(); it != m_service.cluster().end(); ++it) {
            COCAINE_LOG_DEBUG(m_logger, "Add remote to cluster: %s:%d.", it->first, it->second);
            m_cluster.emplace_back(std::make_shared<remote_type>(*this, *it));
        }

        if(m_log.empty()) {
            m_log.append(typename log_type::value_type(0));
        }

        m_election_timer.set<actor, &actor::on_disown>(this);
        m_applier.set<actor, &actor::apply_entries>(this);
    }

    ~actor() {
        reset_election_state();
        finish_leadership();
        stop_election_timer();

        if(m_applier.is_active()) {
            m_applier.stop();
        }
    }

    void
    run() {
        COCAINE_LOG_DEBUG(m_logger, "Running the raft actor.");
        restart_election_timer();
    }

    const std::string&
    name() const {
        return m_name;
    }

    uint64_t
    commit_index() const {
        if(!is_leader() || m_log.last_term() == current_term()) {
            return m_commit_index;
        } else {
            return 0;
        }
    }

    const node_id_t&
    id() const {
        return m_service.id();
    }

    service::raft_t&
    service() {
        return m_service;
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

        COCAINE_LOG_DEBUG(m_logger, "New entry has been added to the log.");

        for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
            try {
                (*it)->replicate();
            } catch(...) {
                // Ignore.
            }
        }

        return true;
    }

private:
    static
    void
    deferred_writer(deferred<std::tuple<uint64_t, bool>> promise,
                    std::function<std::tuple<uint64_t, bool>()> f)
    {
        promise.write(f());
    }

    deferred<std::tuple<uint64_t, bool>>
    append(uint64_t term,
           node_id_t leader,
           std::tuple<uint64_t, uint64_t> prev_entry, // index, term
           const std::vector<msgpack::object>& entries,
           uint64_t commit_index)
    {
        std::vector<typename log_type::value_type> unpacked;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            typename log_type::value_type entry;
            io::type_traits<typename log_type::value_type>::unpack(*it, entry);
            unpacked.emplace_back(std::move(entry));
        }

        deferred<std::tuple<uint64_t, bool>> promise;
        std::function<std::tuple<uint64_t, bool>()> producer = std::bind(&actor::append_impl, this, term, leader, prev_entry, unpacked, commit_index);
        service().reactor().post(std::bind(&actor::deferred_writer, promise, producer));
        return promise;
    }

    deferred<std::tuple<uint64_t, bool>>
    request_vote(uint64_t term,
                 node_id_t candidate,
                 std::tuple<uint64_t, uint64_t> last_entry)
    {
        deferred<std::tuple<uint64_t, bool>> promise;
        std::function<std::tuple<uint64_t, bool>()> producer = std::bind(&actor::request_vote_impl, this, term, candidate, last_entry);
        service().reactor().post(std::bind(&actor::deferred_writer, promise, producer));
        return promise;
    }

    std::tuple<uint64_t, bool>
    append_impl(uint64_t term,
               node_id_t leader,
               std::tuple<uint64_t, uint64_t> prev_entry, // index, term
               const std::vector<typename log_type::value_type>& entries,
               uint64_t commit_index)
    {
        COCAINE_LOG_DEBUG(m_logger,
                          "Append request received from %s:%d, term: %d, prev_entry: (%d, %d), commit index: %d.",
                          leader.first, leader.second,
                          term,
                          std::get<0>(prev_entry), std::get<1>(prev_entry),
                          commit_index);

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
            if(m_log.last_index() > entry_index && it->term() != m_log.at(entry_index).term()) {
                m_log.truncate_suffix(entry_index);
            }

            if(entry_index >= m_log.last_index()) {
                m_log.append(*it);
            }
        }

        m_commit_index = commit_index;

        if(m_commit_index > m_last_applied && !m_applier.is_active()) {
            m_applier.start();
        }

        return std::make_tuple(m_current_term, true);
    }

    std::tuple<uint64_t, bool>
    request_vote_impl(uint64_t term,
                      node_id_t candidate,
                      std::tuple<uint64_t, uint64_t> last_entry)
    {
        COCAINE_LOG_DEBUG(m_logger,
                          "Vote request received from %s:%d, term: %d, last entry: (%d, %d).",
                          candidate.first, candidate.second,
                          term,
                          std::get<0>(last_entry), std::get<1>(last_entry));
        if(term > m_current_term) {
            COCAINE_LOG_DEBUG(m_logger, "Stepping down to term %d from term %d.", term, m_current_term);
            step_down(term);
        }

        if(term == m_current_term &&
           !m_voted_for &&
           (std::get<1>(last_entry) > m_log.last_term() ||
            (std::get<1>(last_entry) == m_log.last_term() &&
             std::get<0>(last_entry) >= m_log.last_index())))
        {
            step_down(term);
            m_voted_for = candidate;
            COCAINE_LOG_DEBUG(m_logger, "In term %d vote granted to %s:%d.", m_current_term, candidate.first, candidate.second);
        }

        return std::make_tuple(m_current_term, term == m_current_term && m_voted_for == candidate);
    }

    void
    step_down(uint64_t term) {
        COCAINE_LOG_DEBUG(m_logger, "Stepping down to term %d.", term);

        if(term > m_current_term) {
            m_current_term = term;
            m_voted_for.reset();
        }

        restart_election_timer();

        // Disable all non-follower activity.
        reset_election_state();
        finish_leadership();

        m_state = state_t::follower;
    }

    struct invocation_visitor_t:
        public boost::static_visitor<>
    {
        invocation_visitor_t(typename log_type::value_type& entry):
            m_entry(entry)
        { }

        template<class Event>
        void
        operator()(const std::shared_ptr<io::basic_slot<Event>>& slot) const {
            (*slot)(boost::get<typename io::aux::frozen<Event>>(m_entry.get_command()).tuple,
                    std::shared_ptr<upstream_t>());
        }

    private:
        typename log_type::value_type& m_entry;
    };

    void
    apply_entries(ev::idle&, int) {
        if(std::min(commit_index(), m_log.last_index()) <= m_last_applied) {
            COCAINE_LOG_DEBUG(m_logger, "Stop applier.");
            m_applier.stop();
            return;
        }

        COCAINE_LOG_DEBUG(m_logger,
                          "Applying entries from %d to %d.",
                          m_last_applied + 1,
                          std::min(commit_index(), m_log.last_index()));

        for(size_t entry = m_last_applied + 1; entry <= std::min(commit_index(), m_log.last_index()); ++entry) {
            if(m_log.at(entry).is_command()) {
                auto& data = m_log.at(entry);
                try {
                    m_state_machine->invoke(data.get_command().which(), invocation_visitor_t(data));
                } catch(const std::exception&) {
                    return;
                }
            }

            ++m_last_applied;
        }
    }

    void
    on_disown(ev::timer&, int) {
        start_election();
    }

    void
    stop_election_timer() {
        if(m_election_timer.is_active()) {
            //COCAINE_LOG_DEBUG(m_logger, "Stop election timer.");
            m_election_timer.stop();
        }
    }

    void
    restart_election_timer() {
        stop_election_timer();

        float election_timeout = m_service.election_timeout() + rand() % m_service.election_timeout();

        //COCAINE_LOG_DEBUG(m_logger, "Election timer will fire in %f milliseconds.", election_timeout);

        m_election_timer.start(election_timeout / 1000.0);
    }

    class election_state_t {
    public:
        election_state_t(actor &actor):
            m_active(true),
            m_granted(1),
            m_actor(actor)
        {
            COCAINE_LOG_DEBUG(m_actor.m_logger, "New election state created.");
        }

        void
        disable() {
            COCAINE_LOG_DEBUG(m_actor.m_logger, "Election state disabled.");
            m_active = false;
        }

        void
        vote_handler(boost::optional<std::tuple<uint64_t, bool>> result) {
            if(!m_active) {
                return;
            }
            COCAINE_LOG_DEBUG(m_actor.m_logger, "New vote result accepted.");

            if(result) {
                COCAINE_LOG_DEBUG(m_actor.m_logger, "Vote accepted: %d, %s", std::get<0>(*result), (std::get<1>(*result) ? "True" : "False"));
                if(std::get<1>(*result)) {
                    m_granted++;

                    if(m_granted > (m_actor.m_cluster.size() + 1) / 2) {
                        m_active = false;
                        m_actor.switch_to_leader();
                    }
                } else if(std::get<0>(*result) > m_actor.current_term()) {
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

    friend class election_state_t;

    void
    start_election() {
        COCAINE_LOG_DEBUG(m_logger, "Start new election.");

        using namespace std::placeholders;

        step_down(m_current_term + 1);

        m_election_state = std::make_shared<election_state_t>(*this);

        m_state = state_t::candidate;

        m_voted_for = id();

        for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
            try {
                (*it)->request_vote(std::bind(&election_state_t::vote_handler, m_election_state, _1));
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
        COCAINE_LOG_DEBUG(m_logger, "Begin leadership.");
        stop_election_timer();
        reset_election_state();

        m_state = state_t::leader;

        call(std::function<void(boost::optional<uint64_t>)>());

        for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
            (*it)->begin_leadership();
        }
    }

    void
    finish_leadership() {
        for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
            (*it)->finish_leadership();
        }

        if(is_leader()) {
            COCAINE_LOG_DEBUG(m_logger, "Finish leadership.");
            for(auto it = m_log.iter(m_commit_index); it != m_log.end(); ++it) {
                it->notify(boost::none);
            }
        }
    }

    static
    bool
    compare_match_index(const std::shared_ptr<remote_type>& left,
                        const std::shared_ptr<remote_type>& right)
    {
        return left->match_index() < right->match_index();
    }

    void
    update_commit_index() {
        size_t pivot = m_cluster.size() % 2 == 0 ? m_cluster.size() / 2 : m_cluster.size() / 2 + 1;
        std::nth_element(m_cluster.begin(),
                         m_cluster.begin() + pivot,
                         m_cluster.end(),
                         &actor::compare_match_index);

        if(m_cluster[pivot]->match_index() > commit_index()) {
            auto old_commit_index = m_commit_index;
            m_commit_index = m_cluster[pivot]->match_index();

            COCAINE_LOG_DEBUG(m_logger, "Commit index has been updated to %d.", m_cluster[pivot]->match_index());

            for(uint64_t i = old_commit_index; i < m_commit_index; ++i) {
                m_log.at(i).notify(i);
            }

            if(!m_applier.is_active()) {
                m_applier.start();
            }
        }
    }

private:
    service::raft_t &m_service;

    const std::unique_ptr<logging::log_t> m_logger;

    std::string m_name;

    std::vector<std::shared_ptr<remote_type>> m_cluster;

    state_t m_state;

    std::shared_ptr<machine_type> m_state_machine;

    log_type m_log;

    uint64_t m_current_term;

    // The highest index known to be commited.
    uint64_t m_commit_index;

    // The last entry applied to the state machine.
    uint64_t m_last_applied;

    boost::optional<node_id_t> m_voted_for;

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
