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

    enum class state_t {
        leader,
        candidate,
        follower
    };

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
          config_type&& config,
          const options_t& options):
        m_context(context),
        m_reactor(reactor),
        m_logger(new logging::log_t(context, "raft/" + name)),
        m_name(name),
        m_options(options),
        m_configuration(std::move(config)),
        m_config_handle(*this, m_configuration),
        m_log(*this, m_configuration.log(), std::move(state_machine)),
        m_cluster(*this),
        m_state(state_t::follower),
        m_booted(false),
        m_received_entries(false),
        m_election_timer(reactor.native())
    {
        COCAINE_LOG_INFO(m_logger, "Initialize Raft actor with name %s.", name);

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

        m_election_timer.set<actor, &actor::on_disown>(this);
    }

    ~actor() {
        stop_election_timer();
        m_cluster.cancel();
        m_joiner.reset();
    }

    void
    join_cluster() {
        COCAINE_LOG_INFO(m_logger, "Joining a cluster.");

        std::vector<node_id_t> remotes;

        auto config_actor = m_context.raft->get("configuration");

        if(config_actor) {
            auto leader = config_actor->leader_id();
            if(leader != node_id_t()) {
                remotes.push_back(leader);
            }
        }

        {
            auto configs = m_context.raft->configuration();

            auto config_machine_iter = configs->find("configuration");

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

        m_joiner = std::make_shared<disposable_client_t>(m_context,
                                                         m_reactor,
                                                         m_context.config.raft.service_name,
                                                         remotes);

        auto success_handler = std::bind(&actor::on_join, this, std::placeholders::_1);
        auto error_handler = std::bind(&actor::join_cluster, this);

        typedef io::raft<msgpack::object, msgpack::object> protocol;

        m_joiner->call<protocol::insert>(success_handler,
                                         error_handler,
                                         name(),
                                         config().id());
    }

    void
    create_cluster() {
        COCAINE_LOG_INFO(m_logger, "Creating new cluster and running the Raft actor.");

        cluster().insert(config().id());
        cluster().commit();

        step_down(std::max<uint64_t>(1, config().current_term()));

        log().reset_snapshot(config().last_applied(),
                             config().current_term(),
                             snapshot_type(log().machine().snapshot(), config().cluster()));

        m_booted = true;
        m_received_entries = true;
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
        return m_options;
    }

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

    machine_type&
    machine() {
        return log().machine();
    }

    const machine_type&
    machine() const {
        return log().machine();
    }

    bool
    is_leader() const {
        return m_state == state_t::leader;
    }

    virtual
    node_id_t
    leader_id() const {
        return *m_leader.synchronize();
    }

    // Send command to the replicated state machine. The actor must be a leader.
    template<class Command, class... Args>
    void
    call(const typename command_traits<Command>::callback_type& handler, Args&&... args) {
        reactor().post(std::bind(
            &actor::call_impl<Command>,
            this->shared_from_this(),
            handler,
            Command(std::forward<Args>(args)...)
        ));
    }

    // Switch to follower state with given term.
    // If reelection is true, then start new elections immediately.
    void
    step_down(uint64_t term, bool reelection = false) {
        BOOST_ASSERT(term >= config().current_term());

        if(term > config().current_term()) {
            COCAINE_LOG_DEBUG(m_logger, "Stepping down to term %d.", term);

            config().set_current_term(term);

            // The actor has not voted in the new term.
            m_voted_for.reset();
        }

        // Disable all non-follower activity.
        m_cluster.cancel();

        if(m_state == state_t::leader) {
            detail::finish_leadership_caller<machine_type>::call(log().machine());
        }

        m_state = state_t::follower;

        restart_election_timer(reelection);
    }

private:
    void
    on_join(const command_result<cluster_change_result>& result) {
        if(!result.error()) {
            m_joiner.reset();
            m_booted = true;
            if(result.value() == cluster_change_result::new_cluster) {
                create_cluster();
            }
        } else {
            join_cluster();
        }
    }

    // Add new command to the log.
    template<class Command>
    void
    call_impl(const typename command_traits<Command>::callback_type& handler, const Command& cmd) {
        if(m_state != state_t::leader) {
            if(handler) {
                handler(std::error_code(raft_errc::not_leader));
            }
            return;
        }

        log().push(config().current_term(), cmd);
        log().template bind_last<Command>(handler);

        COCAINE_LOG_DEBUG(m_logger,
                          "New entry has been added to the log in term %d with index %d.",
                          log().last_term(),
                          log().last_index());
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
        if(cluster().transitional()) {
            promise.write(command_result<void>(raft_errc::busy));
        } else if(cluster().has(node)) {
            promise.write(command_result<void>());
        } else {
            using namespace std::placeholders;

            call_impl<insert_node_t>(
                std::bind(&actor::deferred_setter, this, promise, _1),
                insert_node_t {node}
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
        if(cluster().transitional()) {
            promise.write(command_result<void>(raft_errc::busy));
        } else if(!cluster().has(node)) {
            promise.write(command_result<void>());
        } else {
            using namespace std::placeholders;

            call_impl<erase_node_t>(
                std::bind(&actor::deferred_setter, this, promise, _1),
                erase_node_t {node}
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

        COCAINE_LOG_DEBUG(
            m_logger,
            "Append request received from %s:%d; term: %d; prev_entry: (%d, %d); "
            "entries number: %d; commit index: %d.",
            leader.first, leader.second,
            term,
            prev_index, prev_term,
            entries.size(),
            commit_index
        );

        // Reject stale leader.
        if(term < config().current_term()) {
            COCAINE_LOG_DEBUG(m_logger, "Reject append request from stale leader.");
            return std::make_tuple(config().current_term(), false);
        }

        step_down(term);

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
                COCAINE_LOG_WARNING(
                    m_logger,
                    "Bad append request received from %s:%d; "
                    "term: %d; prev_entry: (%d, %d); commit index: %d.",
                    leader.first, leader.second,
                    term,
                    prev_index, prev_term,
                    commit_index
                );
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
                                  "Term of previous entry doesn't match with the log. "
                                  "Reject the request.");
                return std::make_tuple(config().current_term(), false);
            }
        } else {
            COCAINE_LOG_DEBUG(m_logger,
                              "There is no entry corresponding to prev_entry in the log. "
                              "Reject the request.");
            // Here the log doesn't have entry corresponding to prev_entry, and leader should replicate older entries first.
            return std::make_tuple(config().current_term(), false);
        }

        auto received_before = m_received_entries;

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
                m_received_entries = true;
            }
        }

        config().set_commit_index(commit_index);

        if(received_before != m_received_entries) {
            step_down(config().current_term());
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
        COCAINE_LOG_DEBUG(
            m_logger,
            "Apply request received from %s:%d, term: %d, entry: (%d, %d), commit index: %d.",
            leader.first, leader.second,
            term,
            std::get<0>(snapshot_entry), std::get<1>(snapshot_entry),
            commit_index
        );

        // Reject stale leader.
        if(term < config().current_term()) {
            return std::make_tuple(config().current_term(), false);
        }

        step_down(term);

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

        if(!m_received_entries) {
            m_received_entries = true;
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
                          "Vote request received from %s:%d, term: %d, last entry: (%d, %d).",
                          candidate.first, candidate.second,
                          term,
                          std::get<0>(last_entry), std::get<1>(last_entry));

        if(term > config().current_term()) {
            step_down(term);
        }

        // Check if log of the candidate is as up to date as local log,
        // and vote was not granted to other candidate in the current term.
        if(term == config().current_term() &&
           !m_voted_for &&
           (std::get<1>(last_entry) > log().last_term() ||
            (std::get<1>(last_entry) == log().last_term() &&
             std::get<0>(last_entry) >= log().last_index())))
        {
            step_down(term);
            m_voted_for = candidate;

            COCAINE_LOG_DEBUG(m_logger,
                              "In term %d vote granted to %s:%d.",
                              config().current_term(),
                              candidate.first, candidate.second);

            return std::make_tuple(config().current_term(), true);
        } else {
            return std::make_tuple(config().current_term(), false);
        }
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

        if(m_booted && m_received_entries && cluster().in_cluster()) {
#if defined(__clang__) || defined(HAVE_GCC46)
            typedef std::uniform_int_distribution<unsigned int> uniform_uint;
#else
            typedef std::uniform_int<unsigned int> uniform_uint;
#endif

            uniform_uint distribution(options().election_timeout, 2 * options().election_timeout);

            // We use random timeout to avoid infinite elections in synchronized nodes.
            float timeout = reelection ? 0 : distribution(m_random_generator);
            m_election_timer.start(timeout / 1000.0);

            COCAINE_LOG_DEBUG(m_logger, "Election timer will fire in %f milliseconds.", timeout);
        } else {
            COCAINE_LOG_DEBUG(m_logger, "Not in cluster. Election timer is disabled.");
        }
    }

    void
    on_disown(ev::timer&, int) {
        start_election();
    }

    void
    start_election() {
        COCAINE_LOG_DEBUG(m_logger, "Start new election.");

        using namespace std::placeholders;

        // Start new term.
        step_down(config().current_term() + 1);

        m_state = state_t::candidate;

        // Vote for self.
        m_voted_for = config().id();

        m_cluster.start_election(std::bind(&actor::switch_to_leader, this));
    }

    void
    switch_to_leader() {
        COCAINE_LOG_DEBUG(m_logger, "Begin leadership.");

        // Stop election timer, because we no longer wait messages from leader.
        stop_election_timer();

        m_state = state_t::leader;

        *m_leader.synchronize() = config().id();

        detail::begin_leadership_caller<machine_type>::call(log().machine());

        // Trying to commit NOP entry to assume older entries to be committed
        // (see commitment restriction in the paper).
        call_impl<nop_t>(nullptr, nop_t());

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

    // Actually we need only flag leader/not leader to reject requests, when the actor is not leader.
    state_t m_state;

    // The node for which the actor voted in current term. The node can vote only once in one term.
    boost::optional<node_id_t> m_voted_for;

    // The leader from the last append message.
    // It may be incorrect and actually it's just a tip, where to find current leader.
    synchronized<node_id_t> m_leader;

    // The actor will try to become a leader only if these two variable are true.
    // The first one indicates, that the actor successfully joined cluster via configuration machine.
    // The second one indicates, that the actor has some entries from some former leader.
    bool m_booted;
    bool m_received_entries;

    std::shared_ptr<disposable_client_t> m_joiner;

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
