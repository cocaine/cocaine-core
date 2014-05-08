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

#ifndef COCAINE_RAFT_LOG_HANDLE_HPP
#define COCAINE_RAFT_LOG_HANDLE_HPP

#include "cocaine/detail/raft/forwards.hpp"
#include "cocaine/detail/raft/error.hpp"

#include "cocaine/logging.hpp"

namespace cocaine { namespace raft {

namespace detail {

    template<class Machine, class = void>
    struct complete_log_caller {
        static
        inline
        void
        call(Machine&) {
            // Empty.
        }
    };

    template<class Machine>
    struct complete_log_caller<
        Machine,
        typename aux::require_method<void(Machine::*)(), &Machine::complete_log>::type
    > {
        static
        inline
        void
        call(Machine& machine) {
            machine.complete_log();
        }
    };

} // namespace detail

// This class works with log and state machine and provides snapshotting.
template<class Actor>
class log_handle {
    COCAINE_DECLARE_NONCOPYABLE(log_handle)

    typedef Actor actor_type;
    typedef typename actor_type::config_type config_type;
    typedef typename config_type::log_type log_type;
    typedef typename actor_type::machine_type machine_type;
    typedef typename actor_type::entry_type entry_type;
    typedef typename actor_type::snapshot_type snapshot_type;

public:
    log_handle(actor_type &actor, log_type &log, machine_type&& machine):
        m_actor(actor),
        m_logger(new logging::log_t(actor.context(), "raft/" + actor.name())),
        m_log(log),
        m_machine(std::move(machine)),
        m_next_snapshot_index(0),
        m_next_snapshot_term(0),
        m_background_worker(actor.reactor().native())
    {
        // If the log is empty, then assume that it contains two NOP entries and create initial snapshot.
        // Because all nodes do the same thing, we assume the first entry to be committed.
        // It is not necessary, but it allows me to don't think about corner cases in rest of the code.
        // The snapshot is not committed, because different nodes may start with different configuration.
        // So the second entry is needed to push the leader's configuration to followers.
        if(m_log.empty()) {
            m_log.push(entry_type());
            m_log.push(entry_type());
            m_log.set_snapshot(1, 0, snapshot_type(m_machine.snapshot(), m_actor.config().cluster()));
            actor.config().set_last_applied(1);
            actor.config().set_commit_index(1);
        }

        m_background_worker.set<log_handle, &log_handle::apply_entries>(this);
    }

    bool
    empty() const {
        return m_log.empty();
    }

    entry_type&
    operator[](uint64_t index) {
        return m_log[index];
    }

    const entry_type&
    operator[](uint64_t index) const {
        return m_log[index];
    }

    entry_type&
    back() {
        return m_log[last_index()];
    }

    const entry_type&
    back() const {
        return m_log[last_index()];
    }

    uint64_t
    last_index() const {
        return m_log.last_index();
    }

    uint64_t
    last_term() const {
        return m_log.last_term();
    }

    template<class... Args>
    void
    push(Args&&... args) {
        m_log.push(std::forward<Args>(args)...);

        COCAINE_LOG_DEBUG(m_logger,
                          "new entry has been pushed to the log with index %d and term %d",
                          last_index(),
                          last_term())
        (blackhole::attribute::list({
            {"entry_index", last_index()},
            {"entry_term", last_term()}
        }));

        // Apply new configuration immediately as we see it.
        if(boost::get<io::aux::frozen<node_commands::insert>>(&back().value())) {
            // We need to go deeper.
            m_actor.cluster().insert(std::get<0>(
                boost::get<io::aux::frozen<node_commands::insert>>(back().value()).tuple
            ));
        } else if(boost::get<io::aux::frozen<node_commands::erase>>(&back().value())) {
            m_actor.cluster().erase(std::get<0>(
                boost::get<io::aux::frozen<node_commands::erase>>(back().value()).tuple
            ));
        } else if(boost::get<io::aux::frozen<node_commands::commit>>(&back().value()) &&
                  m_actor.cluster().transitional())
        {
            // Here we can receive spurious commit,
            // if previous commit was truncated from log due to leader change.

            bool was_in_cluster = m_actor.cluster().has(m_actor.config().id());
            m_actor.cluster().commit();

            if(was_in_cluster && !m_actor.cluster().has(m_actor.config().id())) {
                back().set_disable_node(true);
            }
        }

        update_snapshot();
        m_actor.cluster().replicate();
    }

    template<class Event, class Handler>
    void
    bind_last(const Handler& callback) {
        if(std::is_same<Event, node_commands::insert>::value ||
           std::is_same<Event, node_commands::erase>::value)
        {
            m_config_handler = callback;
        } else {
            back().template bind<Event>(callback);
        }
    }

    void
    truncate(uint64_t index) {
        // Now we don't know if truncated entries will be committed,
        // so we provide corresponding error to their handlers.
        // Also we should rollback uncommitted configuration change, if we truncate it.
        for(auto i = last_index(); i >= index; --i) {
            auto &entry = m_log[index];

            if(m_actor.cluster().transitional() &&
               (boost::get<io::aux::frozen<node_commands::insert>>(&entry.value()) ||
                boost::get<io::aux::frozen<node_commands::erase>>(&entry.value())))
            {
                m_actor.cluster().rollback();
                if(m_config_handler) {
                    m_config_handler(std::error_code(raft_errc::unknown));
                    m_config_handler = nullptr;
                }
            }

            entry.set_error(std::error_code(raft_errc::unknown));
        }
        m_log.truncate(index);

        COCAINE_LOG_DEBUG(m_logger, "the log has been truncated to index %d", index)
        (blackhole::attribute::list({
            {"truncate_index", index}
        }));
    }

    void
    reset_snapshot(uint64_t index,
                   uint64_t term,
                   snapshot_type&& snapshot)
    {
        m_log.set_snapshot(index, term, std::move(snapshot));
        m_actor.config().set_last_applied(index - 1);
        m_next_snapshot.reset();

        m_actor.cluster().consume(std::get<1>(m_log.snapshot()));

        COCAINE_LOG_DEBUG(m_logger,
                          "new snapshot has been pushed to the log with index %d and term %d",
                          index,
                          term)
        (blackhole::attribute::list({
            {"snapshot_index", index},
            {"snapshot_term", term}
        }));
    }

    uint64_t
    snapshot_index() const {
        return m_log.snapshot_index();
    }

    uint64_t
    snapshot_term() const {
        return m_log.snapshot_term();
    }

    const snapshot_type&
    snapshot() const {
        return m_log.snapshot();
    }

    // Notify that there is something to apply (usually it means that commit index has been increased).
    void
    apply() {
        if(m_actor.config().last_applied() < m_actor.config().commit_index() &&
           !m_background_worker.is_active())
        {
            m_background_worker.start();
        }
    }

    machine_type&
    machine() {
        return m_machine;
    }

    const machine_type&
    machine() const {
        return m_machine;
    }

private:
    void
    update_snapshot() {
        if(m_next_snapshot &&
           last_index() > m_next_snapshot_index + m_actor.options().snapshot_threshold / 2)
        {
            COCAINE_LOG_DEBUG(
                m_logger,
                "truncate the log up to %d index and save snapshot of the state machine",
                m_next_snapshot_index
            )(blackhole::attribute::list({
                {"snapshot_index", m_next_snapshot_index}
            }));

            if(m_next_snapshot_index > snapshot_index()) {
                m_log.set_snapshot(m_next_snapshot_index,
                                   m_next_snapshot_term,
                                   std::move(*m_next_snapshot));
            }

            m_next_snapshot.reset();
        }
    }

    struct entry_visitor_t {
        machine_type& machine;
        actor_type& actor;
        std::function<void(const std::error_code&)>& config_handler;

        template<class Event>
        typename command_traits<Event>::value_type
        operator()(const io::aux::frozen<Event>& command) const {
            return machine(command);
        }

        void
        operator()(const io::aux::frozen<node_commands::insert>&) const {
            actor.template call<node_commands::commit>(config_handler);
            config_handler = nullptr;
        }

        void
        operator()(const io::aux::frozen<node_commands::erase>&) const {
            actor.template call<node_commands::commit>(config_handler);
            config_handler = nullptr;
        }

        void
        operator()(const io::aux::frozen<node_commands::commit>&) const {
            // Ignore.
        }

        void
        operator()(const io::aux::frozen<node_commands::nop>&) const {
            // Ignore.
        }
    };

    void
    apply_entries(ev::idle&, int) {
        // Compute index of last entry to apply on this iteration.
        // At most options().message_size entries will be applied.
        uint64_t to_apply = std::min(
            m_actor.config().last_applied() + m_actor.options().message_size,
            std::min(m_actor.config().commit_index(), last_index())
        );

        // Stop the watcher when all committed entries are applied.
        if(to_apply <= m_actor.config().last_applied()) {
            COCAINE_LOG_DEBUG(m_logger, "stop applier");
            m_background_worker.stop();
            return;
        }

        COCAINE_LOG_DEBUG(m_logger,
                          "applying entries from %d to %d",
                          m_actor.config().last_applied() + 1,
                          to_apply)
        (blackhole::attribute::list({
            {"first_entry", m_actor.config().last_applied() + 1},
            {"last_entry", to_apply}
        }));

        // Apply snapshot if it's not applied yet.
        // Entries will be applied on the next iterations, because we don't want occupy event loop for a long time.
        if(snapshot_index() > m_actor.config().last_applied()) {
            try {
                m_machine.consume(std::get<0>(m_log.snapshot()));
            } catch(...) {
                return;
            }
            m_actor.config().set_last_applied(snapshot_index());

            return;
        }

        for(size_t index = m_actor.config().last_applied() + 1; index <= to_apply; ++index) {
            auto& entry = m_log[index];
            try {
                if(entry.disable_node()) {
                    m_actor.disable();
                }

                entry.visit(entry_visitor_t {m_machine, m_actor, m_config_handler});
            } catch(const std::exception&) {
                // Unable to apply the entry right now. Try later.
                return;
            }

            m_actor.config().increase_last_applied();

            // If enough entries from previous snapshot are applied, then we should take new snapshot.
            // We will apply this new snapshot to the log later, when there will be enough new entries,
            // because we want to have some amount of entries in the log to replicate them to stale followers.
            if(m_actor.config().last_applied() ==
               snapshot_index() + m_actor.options().snapshot_threshold)
            {
                m_next_snapshot.reset(new snapshot_type(m_machine.snapshot(), m_actor.config().cluster()));
                m_next_snapshot_index = m_actor.config().last_applied();
                m_next_snapshot_term = m_log[m_actor.config().last_applied()].term();

                update_snapshot();
            }
        }

        if(last_index() <= m_actor.config().last_applied()) {
            detail::complete_log_caller<machine_type>::call(m_machine);
        }
    }

private:
    actor_type &m_actor;

    const std::unique_ptr<logging::log_t> m_logger;

    log_type &m_log;

    machine_type m_machine;

    // The snapshot to be written to the log.
    // The handle ensures that log contains at least (snapshot_threshold / 2) entries after a snapshot,
    // because we want to have enough entries to replicate to stale followers.
    // So the handle stores the next snapshot here until the log contains enough entries.
    std::unique_ptr<snapshot_type> m_next_snapshot;

    uint64_t m_next_snapshot_index;

    uint64_t m_next_snapshot_term;

    // This watcher applies committed entries in background.
    ev::idle m_background_worker;

    // Log handler calls this callback, when current configuration change is applied.
    // We don't use callback in log_entry, because configuration change is complex process and
    // requires commitment of two entries.
    std::function<void(const std::error_code&)> m_config_handler;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_LOG_HANDLE_HPP
