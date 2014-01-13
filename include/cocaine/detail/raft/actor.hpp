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

#ifndef COCAINE_RAFT_ACTOR_HPP
#define COCAINE_RAFT_ACTOR_HPP

#include "cocaine/detail/raft/forwards.hpp"
#include "cocaine/detail/raft/remote.hpp"

#include "cocaine/traits/raft.hpp"

#include <memory>
#include <vector>

namespace cocaine { namespace raft {

template<class StateMachine, class Configuration>
class actor:
    public actor_concept_t
{
    typedef actor<StateMachine, Configuration> actor_type;
    typedef remote_node<actor_type> remote_type;

    template<class> friend class remote_node;

public:
    typedef StateMachine machine_type;
    typedef Configuration config_type;
    typedef typename config_type::log_type log_type;
    typedef typename log_type::value_type entry_type;

public:
    template<class ConfigArg>
    actor(context_t& context,
          io::reactor_t& reactor,
          const std::string& name,
          const std::shared_ptr<machine_type>& state_machine,
          ConfigArg&& config,
          uint64_t election_timeout,
          uint64_t heartbeat_timeout):
        m_context(context),
        m_reactor(reactor),
        m_log(new logging::log_t(context, "raft/" + name)),
        m_name(name),
        m_state(state_t::follower),
        m_state_machine(state_machine),
        m_configuration(std::forward<ConfigArg>(config)),
        m_election_timeout(election_timeout),
        m_heartbeat_timeout(heartbeat_timeout),
        m_election_timer(reactor.native()),
        m_applier(reactor.native())
    {
        COCAINE_LOG_DEBUG(m_log, "Initialize raft actor with name %s.", name);

        for(auto it = config().cluster().begin(); it != config().cluster().end(); ++it) {
            m_cluster.emplace_back(std::make_shared<remote_type>(*this, *it));
        }

        if(config().log().empty()) {
            config().log().append(entry_type(0));
        }

        m_election_timer.set<actor, &actor::on_disown>(this);
        m_applier.set<actor, &actor::apply_entries>(this);
    }

    ~actor() {
        stop_election_timer();
        reset_election_state();
        finish_leadership();

        if(m_applier.is_active()) {
            m_applier.stop();
        }
    }

    void
    run() {
        COCAINE_LOG_DEBUG(m_log, "Running the raft actor.");
        step_down(config().current_term() + 1);
    }

    context_t&
    context() {
        return m_context;
    }

    io::reactor_t&
    reactor() {
        return m_reactor;
    }

    const std::string&
    name() const {
        return m_name;
    }

    config_type&
    config() {
        return m_configuration;
    }

    const config_type&
    config() const {
        return m_configuration;
    }

    uint64_t
    election_timeout() const {
        return m_election_timeout;
    }

    uint64_t
    heartbeat_timeout() const {
        return m_heartbeat_timeout;
    }

    uint64_t
    commit_index() const {
        if(!is_leader() || config().log().last_term() == config().current_term()) {
            return config().commit_index();
        } else {
            return 0;
        }
    }

    const node_id_t&
    id() const {
        return config().id();
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

        config().log().append(config().current_term(), std::forward<Args>(args)...);
        config().log().at(config().log().last_index()).bind(std::forward<Handler>(h));

        COCAINE_LOG_DEBUG(m_log, "New entry has been added to the log.");

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
        std::vector<entry_type> unpacked;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            entry_type entry;
            io::type_traits<entry_type>::unpack(*it, entry);
            unpacked.emplace_back(std::move(entry));
        }

        deferred<std::tuple<uint64_t, bool>> promise;
        std::function<std::tuple<uint64_t, bool>()> producer = std::bind(&actor::append_impl, this, term, leader, prev_entry, unpacked, commit_index);
        reactor().post(std::bind(&actor::deferred_writer, promise, producer));
        return promise;
    }

    deferred<std::tuple<uint64_t, bool>>
    request_vote(uint64_t term,
                 node_id_t candidate,
                 std::tuple<uint64_t, uint64_t> last_entry)
    {
        deferred<std::tuple<uint64_t, bool>> promise;
        std::function<std::tuple<uint64_t, bool>()> producer = std::bind(&actor::request_vote_impl, this, term, candidate, last_entry);
        reactor().post(std::bind(&actor::deferred_writer, promise, producer));
        return promise;
    }

    std::tuple<uint64_t, bool>
    append_impl(uint64_t term,
               node_id_t leader,
               std::tuple<uint64_t, uint64_t> prev_entry, // index, term
               const std::vector<entry_type>& entries,
               uint64_t commit_index)
    {
        COCAINE_LOG_DEBUG(m_log,
                          "Append request received from %s:%d, term: %d, prev_entry: (%d, %d), commit index: %d.",
                          leader.first, leader.second,
                          term,
                          std::get<0>(prev_entry), std::get<1>(prev_entry),
                          commit_index);

        if(term < config().current_term()) {
            return std::make_tuple(config().current_term(), false);
        }

        step_down(term);

        m_leader = leader;

        if(std::get<0>(prev_entry) > config().log().last_index()) {
            return std::make_tuple(config().current_term(), false);
        }

        if(std::get<1>(prev_entry) != config().log().at(std::get<0>(prev_entry)).term()) {
            return std::make_tuple(config().current_term(), false);
        }

        uint64_t entry_index = std::get<0>(prev_entry) + 1;
        for(auto it = entries.begin(); it != entries.end(); ++it, ++entry_index) {
            if(config().log().last_index() >= entry_index &&
               it->term() != config().log().at(entry_index).term())
            {
                config().log().truncate_suffix(entry_index);
            }

            if(entry_index > config().log().last_index()) {
                config().log().append(*it);
            }
        }

        config().set_commit_index(commit_index);

        if(config().commit_index() > config().last_applied() && !m_applier.is_active()) {
            m_applier.start();
        }

        return std::make_tuple(config().current_term(), true);
    }

    std::tuple<uint64_t, bool>
    request_vote_impl(uint64_t term,
                      node_id_t candidate,
                      std::tuple<uint64_t, uint64_t> last_entry)
    {
        COCAINE_LOG_DEBUG(m_log,
                          "Vote request received from %s:%d, term: %d, last entry: (%d, %d).",
                          candidate.first, candidate.second,
                          term,
                          std::get<0>(last_entry), std::get<1>(last_entry));
        if(term > config().current_term()) {
            COCAINE_LOG_DEBUG(m_log, "Stepping down to term %d from term %d.", term, m_current_term);
            step_down(term);
        }

        if(term == config().current_term() &&
           !m_voted_for &&
           (std::get<1>(last_entry) > config().log().last_term() ||
            (std::get<1>(last_entry) == config().log().last_term() &&
             std::get<0>(last_entry) >= config().log().last_index())))
        {
            step_down(term);
            m_voted_for = candidate;
            COCAINE_LOG_DEBUG(m_log,
                              "In term %d vote granted to %s:%d.",
                              config().current_term(),
                              candidate.first, candidate.second);
        }

        return std::make_tuple(config().current_term(),
                               term == config().current_term() && m_voted_for == candidate);
    }

    void
    step_down(uint64_t term) {
        COCAINE_LOG_DEBUG(m_log, "Stepping down to term %d.", term);

        if(term > config().current_term()) {
            config().set_current_term(term);
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
        if(std::min(commit_index(), config().log().last_index()) <= config().last_applied()) {
            COCAINE_LOG_DEBUG(m_log, "Stop applier.");
            m_applier.stop();
            return;
        }

        COCAINE_LOG_DEBUG(m_log,
                          "Applying entries from %d to %d.",
                          config().last_applied() + 1,
                          std::min(commit_index(), config().log().last_index()));

        for(size_t entry = config().last_applied() + 1;
            entry <= std::min(commit_index(), config().log().last_index());
            ++entry)
        {
            if(config().log().at(entry).is_command()) {
                auto& data = config().log().at(entry);
                try {
                    m_state_machine->invoke(data.get_command().which(), invocation_visitor_t(data));
                } catch(const std::exception&) {
                    return;
                }
            }

            config().set_last_applied(config().last_applied() + 1);
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

        float timeout = election_timeout() + rand() % election_timeout();

        //COCAINE_LOG_DEBUG(m_logger, "Election timer will fire in %f milliseconds.", election_timeout);

        m_election_timer.start(timeout / 1000.0);
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

                    if(m_granted > (m_actor.config().cluster().size() + 1) / 2) {
                        m_active = false;
                        m_actor.switch_to_leader();
                    }
                } else if(std::get<0>(*result) > m_actor.config().current_term()) {
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
        COCAINE_LOG_DEBUG(m_log, "Start new election.");

        using namespace std::placeholders;

        step_down(config().current_term() + 1);

        m_election_state = std::make_shared<election_state_t>(*this);

        m_state = state_t::candidate;

        m_voted_for = config().id();

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
            for(auto it = config().commit_index(); it <= config().log().last_index(); ++it) {
                config().log().at(it).notify(boost::none);
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
            auto old_commit_index = config().commit_index();
            config().set_commit_index(m_cluster[pivot]->match_index());

            COCAINE_LOG_DEBUG(m_log,
                              "Commit index has been updated to %d.",
                              m_cluster[pivot]->match_index());

            for(uint64_t i = old_commit_index; i < config().commit_index(); ++i) {
                config().log().at(i).notify(i);
            }

            if(!m_applier.is_active()) {
                m_applier.start();
            }
        }
    }

private:
    context_t& m_context;

    io::reactor_t& m_reactor;

    const std::unique_ptr<logging::log_t> m_log;

    std::string m_name;

    std::shared_ptr<machine_type> m_state_machine;

    config_type m_configuration;

    uint64_t m_election_timeout;

    uint64_t m_heartbeat_timeout;

    std::vector<std::shared_ptr<remote_type>> m_cluster;

    state_t m_state;

    // The node for which the actor voted in current term.
    boost::optional<node_id_t> m_voted_for;

    // The leader from the last append message.
    node_id_t m_leader;

    // When the timer fires, the follower will switch to the candidate state.
    ev::timer m_election_timer;

    // This watcher will apply committed entries in background.
    ev::idle m_applier;

    // This state will contain information about received votes and will be reset at finish of the election.
    std::shared_ptr<election_state_t> m_election_state;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_ACTOR_HPP
