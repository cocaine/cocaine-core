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

#include "cocaine/detail/raft/config_handle.hpp"
#include "cocaine/detail/raft/log_handle.hpp"
#include "cocaine/detail/raft/cluster.hpp"
#include "cocaine/detail/raft/client.hpp"
#include "cocaine/detail/raft/forwards.hpp"

#include "cocaine/traits/raft.hpp"

#include "cocaine/platform.hpp"

#include <functional>
#include <memory>
#include <random>
#include <vector>

namespace cocaine { namespace raft {

namespace detail {

    template<class Machine, class = void>
    struct begin_leadership_caller {
        static
        inline
        void
        call(Machine&) {
            // Empty.
        }
    };

    template<class Machine>
    struct begin_leadership_caller<
        Machine,
        typename aux::require_method<void(Machine::*)(), &Machine::begin_leadership>::type
    > {
        static
        inline
        void
        call(Machine& machine) {
            machine.begin_leadership();
        }
    };

    template<class Machine, class = void>
    struct finish_leadership_caller {
        static
        inline
        void
        call(Machine&) {
            // Empty.
        }
    };

    template<class Machine>
    struct finish_leadership_caller<
        Machine,
        typename aux::require_method<void(Machine::*)(), &Machine::finish_leadership>::type
    > {
        static
        inline
        void
        call(Machine& machine) {
            machine.finish_leadership();
        }
    };

} // namespace detail

// Implementation of Raft actor concept (see cocaine/detail/raft/forwards.hpp).
// Actually it is implementation of Raft consensus algorithm.
// It's parametrized with state machine and configuration
// (by default actor is created with empty in-memory log and in-memory configuration).
// User can provide own configuration with various levels of persistence.
template<class StateMachine, class Configuration>
class actor:
    public actor_concept_t,
    public std::enable_shared_from_this<actor<StateMachine, Configuration>>
{
    COCAINE_DECLARE_NONCOPYABLE(actor)

    template<class T> friend class remote_node;

    template<class T> friend class cluster;

    template<class T> friend class log_handle;

    template<class T> friend class config_handle;

    typedef actor<StateMachine, Configuration> actor_type;

    typedef cocaine::raft::cluster<actor_type> cluster_type;

public:
    typedef StateMachine machine_type;

    typedef Configuration config_type;

    typedef log_entry<machine_type> entry_type;

    typedef typename log_traits<machine_type, typename config_type::cluster_type>::snapshot_type
            snapshot_type;

public:
    actor(context_t& context,
          io::reactor_t& reactor,
          const std::string& name, // name of the state machine
          machine_type&& state_machine,
          config_type&& config):
        m_context(context),
        m_reactor(reactor),
        m_logger(new logging::log_t(context, "raft/" + name)),
        m_name(name),
        m_configuration(std::move(config)),
        m_config_handle(*this, m_configuration),
        m_log(*this, m_configuration.log(), std::move(state_machine)),
        m_cluster(*this),
        m_state(actor_state::not_in_cluster),
        m_rejoin_timer(reactor.native()),
        m_election_timer(reactor.native())
    {
        COCAINE_LOG_INFO(m_logger, "initializing raft actor");

#if defined(__clang__) || defined(HAVE_GCC46)
        std::random_device device;
        m_random_generator.seed(device());
#else
        // Initialize the generator with value, which is unique for current node id and time.
        unsigned long random_init = static_cast<unsigned long>(::time(nullptr))
                                  + std::hash<std::string>()(this->context().raft().id().first)
                                  + this->context().raft().id().second;
        m_random_generator.seed(random_init);
#endif

        m_election_timer.set<actor, &actor::on_disown>(this);
        m_rejoin_timer.set<actor, &actor::on_rejoin>(this);
    }

    ~actor() {
        stop_election_timer();

        if(m_rejoin_timer.is_active()) {
            m_rejoin_timer.stop();
        }

        m_cluster.cancel();
        m_joiner.reset();
    }

    virtual
    void
    join_cluster() {
        COCAINE_LOG_INFO(m_logger, "joining the cluster");

        std::vector<node_id_t> remotes;

        auto config_actor = m_context.raft().get(options().configuration_machine_name);

        if(config_actor) {
            auto leader = config_actor->leader_id();
            if(leader != node_id_t()) {
                remotes.push_back(leader);
            }
        }

        {
            auto configs = m_context.raft().configuration();

            auto config_machine_iter = configs->find(options().configuration_machine_name);

            if(config_machine_iter != configs->end()) {
                auto &current_config = config_machine_iter->second.cluster.current;

                std::copy(current_config.begin(),
                          current_config.end(),
                          std::back_inserter(remotes));
            }
        }

        std::copy(options().some_nodes.begin(),
                  options().some_nodes.end(),
                  std::back_inserter(remotes));

        m_joiner.reset();

        m_joiner = std::make_shared<disposable_client_t>(
            m_context,
            m_reactor,
            options().control_service_name,
            remotes
        );

        auto success_handler = std::bind(&actor::on_join, this, std::placeholders::_1);
        auto error_handler = std::bind(&actor::on_join_error, this);

        typedef io::raft_control<msgpack::object, msgpack::object> protocol;

        m_joiner->call<protocol::insert>(success_handler, error_handler, name(), context().raft().id());
    }

    void
    create_cluster() {
        COCAINE_LOG_INFO(m_logger, "creating new cluster and running the raft actor");

        cluster().insert(context().raft().id());
        cluster().commit();

        step_down(std::max<uint64_t>(1, config().current_term()));

        log().reset_snapshot(config().last_applied(),
                             config().current_term(),
                             snapshot_type(log().machine().snapshot(), config().cluster()));

        m_state = actor_state::candidate;
        step_down(config().current_term());
    }

    // Disable the actor. You should call join_cluster method to activate the actor again.
    void
    disable() {
        // Finish leadership correctly if needed.
        if(is_leader()) {
            step_down(config().current_term());
        }

        // The second step down disables election timer.
        m_state = actor_state::not_in_cluster;
        step_down(config().current_term());
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

    const options_t&
    options() const {
        return m_context.raft().options();
    }

    machine_type&
    machine() {
        return log().machine();
    }

    const machine_type&
    machine() const {
        return log().machine();
    }

    virtual
    node_id_t
    leader_id() const {
        return *m_leader.synchronize();
    }

    virtual
    actor_state
    status() const {
        return m_state;
    }

    bool
    is_leader() const {
        return m_state == actor_state::leader;
    }

    // Send command to the replicated state machine. The actor must be a leader.
    template<class Event, class... Args>
    void
    call(const typename command_traits<Event>::callback_type& handler, Args&&... args) {
        reactor().post(std::bind(
            &actor::call_impl<Event>,
            this->shared_from_this(),
            handler,
            io::aux::make_frozen<Event>(std::forward<Args>(args)...)
        ));
    }

private:
    config_handle<actor_type>&
    config() {
        return m_config_handle;
    }

    const config_handle<actor_type>&
    config() const {
        return m_config_handle;
    }

    log_handle<actor_type>&
    log() {
        return m_log;
    }

    const log_handle<actor_type>&
    log() const {
        return m_log;
    }

    cluster_type&
    cluster() {
        return m_cluster;
    }

    const cluster_type&
    cluster() const {
        return m_cluster;
    }

    void
    on_join_error() {
        m_joiner.reset();
        m_rejoin_timer.start(0.5f); // Hardcode!
    }

    void
    on_rejoin(ev::timer&, int) {
        m_rejoin_timer.stop();
        join_cluster();
    }

    void
    on_join(const command_result<cluster_change_result>& result) {
        m_joiner.reset();

        if(!result.error()) {
            if(m_state == actor_state::not_in_cluster) {
                m_state = actor_state::joined;
            } else if(m_state == actor_state::recognized) {
                m_state = actor_state::follower;
            }

            if(result.value() == cluster_change_result::new_cluster) {
                create_cluster();
            } else if(m_state == actor_state::follower) {
                step_down(config().current_term());
            }
        } else {
            join_cluster();
        }
    }

    // Add new command to the log.
    template<class Event>
    void
    call_impl(const typename command_traits<Event>::callback_type& handler,
              const io::aux::frozen<Event>& command)
    {
        if(!is_leader()) {
            if(handler) {
                handler(std::error_code(raft_errc::not_leader));
            }
            return;
        }

        log().push(config().current_term(), command);
        log().template bind_last<Event>(handler);
    }

    // Interface implementation.
    template<class T>
    static
    void
    deferred_pipe(deferred<T> promise, std::function<T()> f) {
        promise.write(f());
    }

    // These methods are just wrappers over implementations,
    // which are posted to one reactor to provide thread-safety.
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
        reactor().post(std::bind(&actor::deferred_pipe<std::tuple<uint64_t, bool>>,
                                 promise,
                                 std::move(producer)));
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
        reactor().post(std::bind(&actor::deferred_pipe<std::tuple<uint64_t, bool>>,
                                 promise,
                                 std::move(producer)));
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
        reactor().post(std::bind(&actor::deferred_pipe<std::tuple<uint64_t, bool>>,
                                 promise,
                                 std::move(producer)));
        return promise;
    }

    void
    deferred_setter(deferred<command_result<void>> promise, const std::error_code& ec) {
        if(ec) {
            promise.write(command_result<void>(static_cast<raft_errc>(ec.value()), leader_id()));
        } else {
            promise.write(command_result<void>());
        }
    }

    virtual
    deferred<command_result<void>>
    insert(const node_id_t& node) {
        deferred<command_result<void>> promise;

        m_reactor.post(std::bind(&actor::insert_impl, this->shared_from_this(), promise, node));

        return promise;
    }

    void
    insert_impl(deferred<command_result<void>> promise, const node_id_t& node) {
        if(!is_leader()) {
            promise.write(command_result<void>(raft_errc::not_leader));
            return;
        }

        if(cluster().transitional()) {
            promise.write(command_result<void>(raft_errc::busy));
        } else if(cluster().has(node)) {
            promise.write(command_result<void>());
        } else {
            using namespace std::placeholders;

            call_impl<node_commands::insert>(
                std::bind(&actor::deferred_setter, this, promise, _1),
                io::aux::make_frozen<node_commands::insert>(node)
            );
        }
    }

    virtual
    deferred<command_result<void>>
    erase(const node_id_t& node) {
        deferred<command_result<void>> promise;

        m_reactor.post(std::bind(&actor::erase_impl, this->shared_from_this(), promise, node));

        return promise;
    }

    void
    erase_impl(deferred<command_result<void>> promise, const node_id_t& node) {
        if(!is_leader()) {
            promise.write(command_result<void>(raft_errc::not_leader));
            return;
        }

        if(cluster().transitional()) {
            promise.write(command_result<void>(raft_errc::busy));
        } else if(!cluster().has(node)) {
            promise.write(command_result<void>());
        } else {
            using namespace std::placeholders;

            call_impl<node_commands::erase>(
                std::bind(&actor::deferred_setter, this, promise, _1),
                io::aux::make_frozen<node_commands::erase>(node)
            );
        }
    }

    // Follower stuff.

    std::tuple<uint64_t, bool>
    append_impl(uint64_t term,
                node_id_t leader,
                std::tuple<uint64_t, uint64_t> prev_entry, // index, term
                const std::vector<entry_type>& entries,
                uint64_t commit_index)
    {
        uint64_t prev_index, prev_term;
        std::tie(prev_index, prev_term) = prev_entry;

        COCAINE_LOG_DEBUG(m_logger,
                          "append request received from %s:%d",
                          leader.first,
                          leader.second)
        (blackhole::attribute::list({
            {"leader_host", leader.first},
            {"leader_port", leader.second},
            {"leader_term", term},
            {"previous_entry_index", prev_index},
            {"previous_entry_term", prev_term},
            {"entries_number", static_cast<uint64_t>(entries.size())},
            {"leader_commit_index", commit_index}
        }));

        // Reject stale leader.
        if(term < config().current_term()) {
            COCAINE_LOG_DEBUG(m_logger, "reject append request from stale leader");
            return std::make_tuple(config().current_term(), false);
        }

        step_down(term);

        m_state = actor_state::follower;
        *m_leader.synchronize() = leader;

        // Check if append is possible and oldest common entries match.
        if(log().snapshot_index() > prev_index &&
           log().snapshot_index() <= prev_index + entries.size())
        {
            // Entry with index snapshot_index is committed and applied.
            // If entry with this index from request has different term,
            // then something is wrong and the actor can't accept this request.
            // Such situation may appear only due to mistake in the Raft implementation.
            uint64_t remote_term = entries[log().snapshot_index() - prev_index - 1].term();

            if(log().snapshot_term() != remote_term) {
                // NOTE: May be we should do std::terminate here to avoid state machine corruption?
                COCAINE_LOG_WARNING(m_logger,
                                    "bad append request received from %s:%d",
                                    leader.first,
                                    leader.second);
                return std::make_tuple(config().current_term(), false);
            }
        } else if(prev_index >= log().snapshot_index() &&
                  prev_index <= log().last_index())
        {
            // prev_entry must match with corresponding entry in local log.
            // Otherwise leader should replicate older entries first.
            uint64_t local_term = log().snapshot_index() == prev_index ?
                                  log().snapshot_term() :
                                  log()[prev_index].term();

            if(local_term != prev_term) {
                COCAINE_LOG_DEBUG(m_logger,
                                  "term of previous entry doesn't match with the log, "
                                  "reject the request");
                return std::make_tuple(config().current_term(), false);
            }
        } else {
            COCAINE_LOG_DEBUG(m_logger,
                              "there is no entry corresponding to prev_entry in the log, "
                              "reject the request");
            // Here the log doesn't have entry corresponding to prev_entry, and leader should replicate older entries first.
            return std::make_tuple(config().current_term(), false);
        }

        bool pushed_some_entries = false;

        // Append.
        uint64_t entry_index = prev_index + 1;
        for(auto it = entries.begin(); it != entries.end(); ++it, ++entry_index) {
            // If terms of entries don't match, then actor must replace local entries with ones from leader.
            if(entry_index > log().snapshot_index() &&
               entry_index <= log().last_index() &&
               it->term() != log()[entry_index].term())
            {
                log().truncate(entry_index);
            }

            if(entry_index > log().last_index()) {
                log().push(*it);
                pushed_some_entries = true;
            }
        }

        config().set_commit_index(commit_index);

        if(pushed_some_entries) {
            if(m_state == actor_state::not_in_cluster) {
                m_state = actor_state::recognized;
            } else if(m_state == actor_state::joined) {
                m_state = actor_state::follower;
                step_down(config().current_term());
            }
        }

        return std::make_tuple(config().current_term(), true);
    }

    std::tuple<uint64_t, bool>
    apply_impl(uint64_t term,
               node_id_t leader,
               std::tuple<uint64_t, uint64_t> snapshot_entry, // index, term
               const snapshot_type& snapshot,
               uint64_t commit_index)
    {
        COCAINE_LOG_DEBUG(m_logger,
                          "apply request received from %s:%d",
                          leader.first,
                          leader.second)
        (blackhole::attribute::list({
            {"leader_host", leader.first},
            {"leader_port", leader.second},
            {"leader_term", term},
            {"snapshot_index", std::get<0>(snapshot_entry)},
            {"snapshot_term", std::get<1>(snapshot_entry)},
            {"leader_commit_index", commit_index}
        }));

        // Reject stale leader.
        if(term < config().current_term()) {
            return std::make_tuple(config().current_term(), false);
        }

        step_down(term);

        m_state = actor_state::follower;
        *m_leader.synchronize() = leader;

        // Truncate wrong entries.
        if(std::get<0>(snapshot_entry) > log().snapshot_index() &&
           std::get<0>(snapshot_entry) <= log().last_index() &&
           std::get<1>(snapshot_entry) != log()[std::get<0>(snapshot_entry)].term())
        {
            // If term of entry corresponding to snapshot doesn't match snapshot term,
            // then actor should replace local entries with ones from leader.
            log().truncate(std::get<0>(snapshot_entry));
        }

        log().reset_snapshot(std::get<0>(snapshot_entry),
                             std::get<1>(snapshot_entry),
                             snapshot_type(snapshot));
        config().set_last_applied(std::get<0>(snapshot_entry) - 1);
        config().set_commit_index(commit_index);

        if(m_state == actor_state::not_in_cluster) {
            m_state = actor_state::recognized;
        } else if(m_state == actor_state::joined) {
            m_state = actor_state::follower;
            step_down(config().current_term());
        }

        return std::make_tuple(config().current_term(), true);
    }

    std::tuple<uint64_t, bool>
    request_vote_impl(uint64_t term,
                      node_id_t candidate,
                      std::tuple<uint64_t, uint64_t> last_entry)
    {
        COCAINE_LOG_DEBUG(m_logger,
                          "vote request received from %s:%d",
                          candidate.first,
                          candidate.second)
        (blackhole::attribute::list({
            {"candidate_host", candidate.first},
            {"candidate_port", candidate.second},
            {"candidate_term", term},
            {"last_entry_index", std::get<0>(last_entry)},
            {"last_entry_term", std::get<1>(last_entry)}
        }));

        // Check if log of the candidate is as up to date as local log,
        // and vote was not granted to other candidate in the current term.
        if(std::get<1>(last_entry) > log().last_term() ||
           (std::get<1>(last_entry) == log().last_term() &&
            std::get<0>(last_entry) >= log().last_index()))
        {
            step_down(term);

            if(term == config().current_term() && !m_voted_for) {
                m_voted_for = candidate;

                COCAINE_LOG_DEBUG(m_logger,
                                  "in term %d vote granted to %s:%d",
                                  config().current_term(),
                                  candidate.first,
                                  candidate.second);

                return std::make_tuple(config().current_term(), true);
            }
        } else if(term > config().current_term() && cluster().has(candidate)) {
            // Don't step down if the candidate has old log and is not in the cluster.
            // Probably it's stale node, which is already removed but doesn't know it.
            // So it shouldn't initiate meaningless reelections.
            step_down(term);
        }

        return std::make_tuple(config().current_term(), false);
    }

    // Switch to follower state with given term.
    // If reelection is true, then start new elections immediately.
    void
    step_down(uint64_t term, bool reelection = false) {
        BOOST_ASSERT(term >= config().current_term());

        if(term > config().current_term()) {
            COCAINE_LOG_DEBUG(m_logger, "stepping down to term %d", term)
            (blackhole::attribute::list({
                {"term", term}
            }));

            config().set_current_term(term);

            // The actor has not voted in the new term.
            m_voted_for.reset();
        }

        // Disable all non-follower activity.
        m_cluster.cancel();

        if(is_leader()) {
            m_state = actor_state::candidate;
            *m_leader.synchronize() = node_id_t();
            detail::finish_leadership_caller<machine_type>::call(log().machine());
        }

        restart_election_timer(reelection);
    }

    // Election.

    void
    stop_election_timer() {
        if(m_election_timer.is_active()) {
            m_election_timer.stop();
        }
    }

    // If reelection is true, then start new elections immediately.
    void
    restart_election_timer(bool reelection) {
        stop_election_timer();

        bool booted = m_state == actor_state::follower ||
                      m_state == actor_state::candidate ||
                      m_state == actor_state::leader;

        if(booted) {
#if defined(__clang__) || defined(HAVE_GCC46)
            typedef std::uniform_int_distribution<unsigned int> uniform_uint;
#else
            typedef std::uniform_int<unsigned int> uniform_uint;
#endif

            uniform_uint distribution(options().election_timeout, 2 * options().election_timeout);

            // We use random timeout to avoid infinite elections in synchronized nodes.
            float timeout = reelection ? 0 : distribution(m_random_generator);
            m_election_timer.start(timeout / 1000.0);

            COCAINE_LOG_DEBUG(m_logger, "election timer will fire in %f milliseconds", timeout)
            (blackhole::attribute::list({
                {"election_timeout", timeout}
            }));
        } else {
            COCAINE_LOG_DEBUG(m_logger, "not in cluster, election timer is disabled");
        }
    }

    void
    on_disown(ev::timer&, int) {
        start_election();
    }

    void
    start_election() {
        COCAINE_LOG_DEBUG(m_logger, "start new election");

        using namespace std::placeholders;

        // Start new term.
        step_down(config().current_term() + 1);

        // Vote for self.
        m_voted_for = context().raft().id();

        m_state = actor_state::candidate;

        m_cluster.start_election(std::bind(&actor::switch_to_leader, this));
    }

    void
    switch_to_leader() {
        COCAINE_LOG_DEBUG(m_logger, "begin leadership");

        // Stop election timer, because we no longer wait messages from leader.
        stop_election_timer();

        m_state = actor_state::leader;

        *m_leader.synchronize() = context().raft().id();

        detail::begin_leadership_caller<machine_type>::call(log().machine());

        // Trying to commit NOP entry to assume older entries to be committed
        // (see commitment restriction in the paper).
        call_impl<node_commands::nop>(nullptr, io::aux::make_frozen<node_commands::nop>());

        m_cluster.begin_leadership();
    }

private:
    context_t& m_context;

    io::reactor_t& m_reactor;

    const std::unique_ptr<logging::log_t> m_logger;

    std::string m_name;

    options_t m_options;

    config_type m_configuration;

    config_handle<actor_type> m_config_handle;

    log_handle<actor_type> m_log;

    cluster_type m_cluster;

    actor_state m_state;

    // The node for which the actor voted in current term. The node can vote only once in one term.
    boost::optional<node_id_t> m_voted_for;

    // The leader from the last append message.
    // It may be incorrect and actually it's just a tip, where to find current leader.
    synchronized<node_id_t> m_leader;

    std::shared_ptr<disposable_client_t> m_joiner;

    ev::timer m_rejoin_timer;

    // When the timer fires, the follower will switch to the candidate state.
    ev::timer m_election_timer;

#if defined(__clang__) || defined(HAVE_GCC46)
    std::default_random_engine m_random_generator;
#else
    std::minstd_rand0 m_random_generator;
#endif
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_ACTOR_HPP
