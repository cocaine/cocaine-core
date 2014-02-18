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
#include "cocaine/detail/raft/error.hpp"

#include "cocaine/traits/raft.hpp"

#include "cocaine/platform.hpp"

#include <functional>
#include <memory>
#include <random>
#include <vector>

namespace cocaine { namespace raft {

// Implementation of Raft actor concept (see cocaine/detail/raft/forwards.hpp).
// Actually it is implementation of Raft consensus algorithm.
// It's parametrized with state machine and configuration (by default actor is created with empty in-memory log and in-memory configuration).
// User can provide own configuration with various levels of persistence.
template<class StateMachine, class Configuration>
class actor:
    public actor_concept_t,
    public std::enable_shared_from_this<actor<StateMachine, Configuration>>
{
    COCAINE_DECLARE_NONCOPYABLE(actor)

    enum class state_t {
        leader,
        candidate,
        follower
    };

    typedef actor<StateMachine, Configuration> actor_type;
    typedef remote_node<actor_type> remote_type;

    template<class> friend class remote_node;

public:
    typedef StateMachine machine_type;
    typedef Configuration config_type;
    typedef typename config_type::log_type log_type;
    typedef log_entry<machine_type> entry_type;
    typedef typename machine_type::snapshot_type snapshot_type;

public:
    actor(context_t& context,
          io::reactor_t& reactor,
          const std::string& name, // name of the state machine
          machine_type&& state_machine,
          config_type&& config,
          const options_t& options):
        m_context(context),
        m_reactor(reactor),
        m_log(new logging::log_t(context, "raft/" + name)),
        m_name(name),
        m_machine(std::move(state_machine)),
        m_configuration(std::move(config)),
        m_options(options),
        m_state(state_t::follower),
        m_election_timer(reactor.native()),
        m_applier(reactor.native()),
        m_replicator(reactor.native())
    {
        COCAINE_LOG_INFO(m_log, "Initialize Raft actor with name %s.", name);

#if defined(__clang__) || defined(HAVE_GCC46)
        std::random_device device;
        m_random_generator.seed(device());
#else
        // Initialize the generator with value, which is unique for current node id and time.
        unsigned long random_init = static_cast<unsigned long>(::time(nullptr))
                                  + std::hash<std::string>()(this->config().id().first)
                                  + this->config().id().second;
        m_random_generator.seed(random_init);
#endif

        // If the log is empty, then assume that it contains one NOP entry and create initial snapshot.
        // Because all nodes do the same thing, this entry is always committed.
        // It is not necessary, but it allows me to don't think about corner cases in rest of code.
        if(this->config().log().empty()) {
            this->config().log().push();
            this->config().log().set_snapshot(0, 0, m_machine.snapshot());
            this->config().set_commit_index(0);
            this->config().set_last_applied(0);
        }

        m_election_timer.set<actor, &actor::on_disown>(this);
        m_applier.set<actor, &actor::apply_entries>(this);
        m_replicator.set<actor, &actor::replicate>(this);

        // Create handlers for other nodes in the cluster.
        for(auto it = this->config().cluster().begin();
            it != this->config().cluster().end();
            ++it)
        {
            if(*it != this->config().id()) {
                m_cluster.emplace_back(std::make_shared<remote_type>(*this, *it));
            }
        }
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
        COCAINE_LOG_INFO(m_log, "Running the Raft actor.");
        step_down(config().current_term());
    }

    const std::string&
    name() const {
        return m_name;
    }

    const options_t&
    options() const {
        return m_options;
    }

    node_id_t
    leader_id() const {
        return *m_leader.synchronize();
    }

    // Send command to the replicated state machine. The actor must be a leader.
    template<class Event, class... Args>
    void
    call(const typename command_traits<Event>::callback_type& handler, Args&&... args) {
        typedef typename entry_type::command_type cmd_type;

        reactor().post(std::bind(
            &actor::call_impl<Event>,
            this->shared_from_this(),
            handler,
            cmd_type(io::aux::frozen<Event>(Event(), std::forward<Args>(args)...))
        ));
    }

private:
    context_t&
    context() {
        return m_context;
    }

    io::reactor_t&
    reactor() {
        return m_reactor;
    }

    config_type&
    config() {
        return m_configuration;
    }

    const config_type&
    config() const {
        return m_configuration;
    }

    bool
    is_leader() const {
        return m_state == state_t::leader;
    }

    // Add new command to the log.
    template<class Event>
    void
    call_impl(const typename command_traits<Event>::callback_type& handler,
              const typename entry_type::command_type& cmd)
    {
        if(!is_leader()) {
            if(handler) {
                handler(std::error_code(raft_errc::not_leader));
            }
            return;
        }

        config().log().push(config().current_term(), cmd);
        config().log()[config().log().last_index()].template bind<Event>(handler);

        replace_snapshot();

        if(!m_replicator.is_active()) {
            m_replicator.start();
        }

        COCAINE_LOG_DEBUG(m_log,
                          "New entry has been added to the log in term %d with index %d.",
                          config().log().last_term(),
                          config().log().last_index());
    }

    // Add nop entry to the log.
    void
    add_nop() {
        config().log().push(config().current_term(), nop_t());

        replace_snapshot();

        if(!m_replicator.is_active()) {
            m_replicator.start();
        }

        COCAINE_LOG_DEBUG(m_log,
                          "New nop entry has been added to the log in term %d with index %d.",
                          config().log().last_term(),
                          config().log().last_index());
    }

    // 'call' method starts this background task.
    // When all requests in reactor queue are processed, event-loop calls this method, which replicates the new entries to remote nodes.
    // Idle watcher is just a way to push many entries to the log and then replicate them at once.
    void
    replicate(ev::idle&, int) {
        m_replicator.stop();

        for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
            try {
                (*it)->replicate();
            } catch(...) {
                // Ignore.
            }
        }
    }

    // Follower stuff.

    static
    void
    deferred_writer(deferred<std::tuple<uint64_t, bool>> promise,
                    std::function<std::tuple<uint64_t, bool>()> f)
    {
        promise.write(f());
    }

    // These methods are just wrappers over implementations, which are posted to one reactor to provide thread-safety.
    virtual
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
        std::function<std::tuple<uint64_t, bool>()> producer = std::bind(
            &actor::append_impl,
            this->shared_from_this(),
            term,
            leader,
            prev_entry,
            unpacked,
            commit_index
        );
        reactor().post(std::bind(&actor::deferred_writer, promise, std::move(producer)));
        return promise;
    }

    virtual
    deferred<std::tuple<uint64_t, bool>>
    apply(uint64_t term,
          node_id_t leader,
          std::tuple<uint64_t, uint64_t> snapshot_entry, // index, term
          const msgpack::object& snapshot,
          uint64_t commit_index)
    {
        snapshot_type unpacked;
        io::type_traits<snapshot_type>::unpack(snapshot, unpacked);

        deferred<std::tuple<uint64_t, bool>> promise;
        std::function<std::tuple<uint64_t, bool>()> producer = std::bind(
            &actor::apply_impl,
            this->shared_from_this(),
            term,
            leader,
            snapshot_entry,
            unpacked,
            commit_index
        );
        reactor().post(std::bind(&actor::deferred_writer, promise, std::move(producer)));
        return promise;
    }

    virtual
    deferred<std::tuple<uint64_t, bool>>
    request_vote(uint64_t term,
                 node_id_t candidate,
                 std::tuple<uint64_t, uint64_t> last_entry)
    {
        deferred<std::tuple<uint64_t, bool>> promise;
        std::function<std::tuple<uint64_t, bool>()> producer = std::bind(
            &actor::request_vote_impl,
            this->shared_from_this(),
            term,
            candidate,
            last_entry
        );
        reactor().post(std::bind(&actor::deferred_writer, promise, std::move(producer)));
        return promise;
    }

    std::tuple<uint64_t, bool>
    append_impl(uint64_t term,
                node_id_t leader,
                std::tuple<uint64_t, uint64_t> prev_entry, // index, term
                const std::vector<entry_type>& entries,
                uint64_t commit_index)
    {
        uint64_t prev_index, prev_term;
        std::tie(prev_index, prev_term) = prev_entry;

        COCAINE_LOG_DEBUG(m_log,
                          "Append request received from %s:%d, term: %d, prev_entry: (%d, %d), commit index: %d.",
                          leader.first, leader.second,
                          term,
                          prev_index, prev_term,
                          commit_index);

        // Reject stale leader.
        if(term < config().current_term()) {
            return std::make_tuple(config().current_term(), false);
        }

        step_down(term);

        *m_leader.synchronize() = leader;

        // Check if append is possible and oldest common entries match.
        if(config().log().snapshot_index() > prev_index &&
           config().log().snapshot_index() <= prev_index + entries.size())
        {
            // Entry with index snapshot_index is committed and applied.
            // If entry with this index from request has different term, then something is wrong and the actor can't accept this request.
            // Such situation may appear only due to mistake in the Raft implementation.
            uint64_t remote_term = entries[config().log().snapshot_index() - prev_index - 1].term();

            if(config().log().snapshot_term() != remote_term) {
                // NOTE: May be we should do std::terminate here to avoid state machine corruption?
                COCAINE_LOG_WARNING(
                    m_log,
                    "Bad append request received from %s:%d, term: %d, prev_entry: (%d, %d), commit index: %d.",
                    leader.first, leader.second,
                    term,
                    prev_index, prev_term,
                    commit_index
                );
                return std::make_tuple(config().current_term(), false);
            }
        } else if(prev_index >= config().log().snapshot_index() &&
                  prev_index <= config().log().last_index())
        {
            // prev_entry must match with corresponding entry in local log.
            // Otherwise leader should replicate older entries first.
            uint64_t local_term = config().log().snapshot_index() == prev_index ?
                                  config().log().snapshot_term() :
                                  config().log()[prev_index].term();

            if(local_term != prev_term) {
                return std::make_tuple(config().current_term(), false);
            }
        } else {
            // Here the log doesn't have entry corresponding to prev_entry, and leader should replicate older entries first.
            return std::make_tuple(config().current_term(), false);
        }

        // Append.
        uint64_t entry_index = prev_index + 1;
        for(auto it = entries.begin(); it != entries.end(); ++it, ++entry_index) {
            // If terms of entries don't match, then actor must replace local entries with ones from leader.
            if(entry_index > config().log().snapshot_index() &&
               entry_index <= config().log().last_index() &&
               it->term() != config().log()[entry_index].term())
            {
                config().log().truncate(entry_index);
            }

            if(entry_index > config().log().last_index()) {
                config().log().push(*it);
            }
        }

        replace_snapshot();

        set_commit_index(commit_index);

        return std::make_tuple(config().current_term(), true);
    }

    std::tuple<uint64_t, bool>
    apply_impl(uint64_t term,
               node_id_t leader,
               std::tuple<uint64_t, uint64_t> snapshot_entry, // index, term
               const snapshot_type& snapshot,
               uint64_t commit_index)
    {
        COCAINE_LOG_DEBUG(m_log,
                          "Apply request received from %s:%d, term: %d, entry: (%d, %d), commit index: %d.",
                          leader.first, leader.second,
                          term,
                          std::get<0>(snapshot_entry), std::get<1>(snapshot_entry),
                          commit_index);

        // Reject stale leader.
        if(term < config().current_term()) {
            return std::make_tuple(config().current_term(), false);
        }

        step_down(term);

        *m_leader.synchronize() = leader;

        // Truncate wrong entries.
        if(std::get<0>(snapshot_entry) > config().log().snapshot_index() &&
           std::get<0>(snapshot_entry) <= config().log().last_index() &&
           std::get<1>(snapshot_entry) != config().log()[std::get<0>(snapshot_entry)].term())
        {
            // If term of entry corresponding to snapshot doesn't match snapshot term, then actor should replace local entries with ones from leader.
            config().log().truncate(std::get<0>(snapshot_entry));
        }

        config().log().set_snapshot(std::get<0>(snapshot_entry),
                                    std::get<1>(snapshot_entry),
                                    snapshot);
        config().set_last_applied(std::get<0>(snapshot_entry) - 1);

        set_commit_index(commit_index);

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
            step_down(term);
        }

        // Check if log of the candidate is as up to date as local log, and vote was not granted to other candidate in the current term.
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

            return std::make_tuple(config().current_term(), true);
        } else {
            return std::make_tuple(config().current_term(), false);
        }
    }

    // Update snapshot in the log, if there are enough entries after new snapshot.
    // This method should be called, when new entries are added to the log.
    void
    replace_snapshot() {
        if(m_next_snapshot &&
           config().log().last_index() > m_snapshot_index + options().snapshot_threshold / 2)
        {
            COCAINE_LOG_DEBUG(m_log,
                              "Truncate the log up to %d index and save snapshot of the state machine.",
                              m_snapshot_index);

            if(m_snapshot_index > config().log().snapshot_index()) {
                config().log().set_snapshot(m_snapshot_index,
                                            m_snapshot_term,
                                            std::move(*m_next_snapshot));
            }

            m_next_snapshot.reset();
        }
    }

    // Switch to follower state with given term.
    void
    step_down(uint64_t term) {
        if(term > config().current_term()) {
            COCAINE_LOG_DEBUG(m_log, "Stepping down to term %d.", term);

            config().set_current_term(term);

            // The actor has not voted in the new term.
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
        invocation_visitor_t(machine_type& machine, entry_type& entry):
            m_machine(machine),
            m_entry(entry)
        { }

        template<class Event>
        void
        operator()(const io::aux::frozen<Event>& command) const {
                m_entry.template notify<Event>(m_machine(command));
        }

    private:
        machine_type& m_machine;
        entry_type& m_entry;
    };

    // This background task applies committed entries to the state machine.
    void
    apply_entries(ev::idle&, int) {
        // Compute index of last entry to apply on this iteration.
        // At most options().message_size entries will be applied.
        uint64_t to_apply = std::min(config().last_applied() + options().message_size,
                                     std::min(config().commit_index(), config().log().last_index()));

        // Stop the watcher when all committed entries are applied.
        if(to_apply <= config().last_applied()) {
            COCAINE_LOG_DEBUG(m_log, "Stop applier.");
            m_applier.stop();
            return;
        }

        COCAINE_LOG_DEBUG(m_log,
                          "Applying entries from %d to %d.",
                          config().last_applied() + 1,
                          to_apply);

        // Apply snapshot if it's not applied yet.
        // Entries will be applied on the next iterations, because we don't want occupy event loop for a long time.
        if(config().log().snapshot_index() > config().last_applied()) {
            try {
                m_machine.consume(config().log().snapshot());
            } catch(...) {
                return;
            }
            config().set_last_applied(config().log().snapshot_index());

            return;
        }

        for(size_t entry = config().last_applied() + 1; entry <= to_apply; ++entry) {
            auto& data = config().log()[entry];
            if(data.is_command()) {
                try {
                    boost::apply_visitor(invocation_visitor_t(m_machine, data), data.get_command());
                } catch(const std::exception&) {
                    // Unable to apply the entry right now. Try later.
                    return;
                }
            }

            config().set_last_applied(config().last_applied() + 1);

            // If enough entries from previous snapshot are applied, then we should take new snapshot.
            // We will apply this new snapshot to the log later, when there will be enough new entries,
            // because we want to have some amount of entries in the log to replicate them to stale followers.
            if(config().last_applied() == config().log().snapshot_index() + options().snapshot_threshold) {
                m_next_snapshot.reset(new snapshot_type(m_machine.snapshot()));
                m_snapshot_index = config().last_applied();
                m_snapshot_term = config().log()[config().last_applied()].term();
            }
        }
    }

    // Election.

    void
    on_disown(ev::timer&, int) {
        start_election();
    }

    void
    stop_election_timer() {
        if(m_election_timer.is_active()) {
            m_election_timer.stop();
        }
    }

    void
    restart_election_timer() {
        stop_election_timer();

#if defined(__clang__) || defined(HAVE_GCC46)
        typedef std::uniform_int_distribution<unsigned int> uniform_uint;
#else
        typedef std::uniform_int<unsigned int> uniform_uint;
#endif

        uniform_uint distribution(options().election_timeout, 2 * options().election_timeout);

        // We use random timeout to avoid infinite elections in synchronized nodes.
        float timeout = distribution(m_random_generator);
        m_election_timer.start(timeout / 1000.0);

        COCAINE_LOG_DEBUG(m_log, "Election timer will fire in %f milliseconds.", timeout);
    }

    // Instance of this class counts obtained votes and moves the actor to leader state, when it obtains votes from quorum.
    class election_state_t {
    public:
        election_state_t(actor &actor):
            m_active(true),
            m_granted(1),
            m_actor(actor)
        { }

        // When the actor doesn't need results of the election, it disables the state.
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
                COCAINE_LOG_DEBUG(m_actor.m_log,
                                  "New vote accepted: %d, %s",
                                  std::get<0>(*result),
                                  (std::get<1>(*result) ? "True" : "False"));

                if(std::get<1>(*result)) {
                    m_granted++;

                    if(m_granted > (m_actor.config().cluster().size() + 1) / 2) {
                        // Actor has won the election.
                        m_actor.switch_to_leader();
                    }
                } else if(std::get<0>(*result) > m_actor.config().current_term()) {
                    // Actor lives in the past. Stepdown to follower with new term.
                    m_actor.step_down(std::get<0>(*result));
                }
            } else {
                COCAINE_LOG_DEBUG(m_actor.m_log, "Vote request has failed.");
            }
        }

    private:
        bool m_active;
        unsigned int m_granted;
        actor &m_actor;
    };

    void
    start_election() {
        COCAINE_LOG_DEBUG(m_log, "Start new election.");

        using namespace std::placeholders;

        // Start new term.
        step_down(config().current_term() + 1);

        m_election_state = std::make_shared<election_state_t>(*this);

        m_state = state_t::candidate;

        // Vote for self.
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
        COCAINE_LOG_DEBUG(m_log, "Begin leadership.");

        // Stop election timer, because we no longer wait messages from leader.
        stop_election_timer();
        reset_election_state();

        m_state = state_t::leader;

        // Trying to commit NOP entry to assume older entries committed (see commitment restriction in the paper).
        add_nop();

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
            COCAINE_LOG_DEBUG(m_log, "Finish leadership.");

            // If leader becomes follower, uncommitted entries may eventually become committed or discarded.
            // So leader notifies handler with corresponding error code.
            for(auto i = config().last_applied() + 1; i <= config().log().last_index(); ++i) {
                config().log()[i].notify(std::error_code(raft_errc::unknown));
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

    // Compute new commit index based on information about replicated entries.
    void
    update_commit_index() {
        // Find median in nodes match_index'es. This median is index of last entry replicated to quorum.
        size_t pivot = (m_cluster.size() % 2 == 0) ? (m_cluster.size() / 2) : (m_cluster.size() / 2 + 1);
        std::nth_element(m_cluster.begin(),
                         m_cluster.begin() + pivot,
                         m_cluster.end(),
                         &actor::compare_match_index);

        uint64_t just_commited = m_cluster[pivot]->match_index();

        // Leader can't assume any new entry to be committed until entry from current term is replicated to quorum
        // (see commitment restriction in the paper).
        if(just_commited > config().commit_index() &&
           config().log()[just_commited].term() == config().current_term())
        {
            set_commit_index(just_commited);
        }
    }

    // Update commit index to new value and start applier, if it's needed.
    void
    set_commit_index(uint64_t index) {
        auto new_index = std::min(index, config().log().last_index());

        config().set_commit_index(new_index);

        COCAINE_LOG_DEBUG(m_log, "Commit index has been updated to %d.", new_index);

        if(config().last_applied() < config().commit_index() && !m_applier.is_active()) {
            m_applier.start();
        }
    }

private:
    context_t& m_context;

    io::reactor_t& m_reactor;

    const std::unique_ptr<logging::log_t> m_log;

    std::string m_name;

    machine_type m_machine;

    config_type m_configuration;

    options_t m_options;

    std::vector<std::shared_ptr<remote_type>> m_cluster;

    // Actually we need only flag leader/not leader to reject requests, when the actor is not leader.
    state_t m_state;

    // The node for which the actor voted in current term. The node can vote only once in one term.
    boost::optional<node_id_t> m_voted_for;

    // The leader from the last append message.
    // It may be incorrect and actually it's just a tip, where to find current leader.
    synchronized<node_id_t> m_leader;

    // When the timer fires, the follower will switch to the candidate state.
    ev::timer m_election_timer;

    // This watcher will apply committed entries in background.
    ev::idle m_applier;

    // This watcher replicates entries in background.
    ev::idle m_replicator;

    // This state will contain information about received votes and will be reset at finish of the election.
    std::shared_ptr<election_state_t> m_election_state;

    // The snapshot to be written to the log.
    // The actor ensures that log contains at least (m_snapshot_threshold / 2) entries after a snapshot,
    // because we want to have enough entries to replicate to stale followers.
    // So the actor stores the next snapshot here until the log contains enough entries.
    std::unique_ptr<snapshot_type> m_next_snapshot;

    uint64_t m_snapshot_index;

    uint64_t m_snapshot_term;

#if defined(__clang__) || defined(HAVE_GCC46)
    std::default_random_engine m_random_generator;
#else
    std::minstd_rand0 m_random_generator;
#endif
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_ACTOR_HPP
