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
        // If the log is empty, then assume that it contains one NOP entry and create initial snapshot.
        // Because all nodes do the same thing, this entry is always committed.
        // It is not necessary, but it allows me to don't think about corner cases in rest of code.
        if(m_log.empty()) {
            m_log.push(entry_type());
            m_log.set_snapshot(0, 0, snapshot_type(m_machine.snapshot(), m_actor.config().cluster()));
            actor.config().set_last_applied(0);
            actor.config().set_commit_index(0);
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

        // Apply new configuration immediately as we see it.
        if (boost::get<insert_node_t>(&back().value())) {
            m_actor.cluster().insert(boost::get<insert_node_t>(back().value()).node);
        } else if (boost::get<erase_node_t>(&back().value())) {
            m_actor.cluster().erase(boost::get<erase_node_t>(back().value()).node);
        } else if (boost::get<commit_node_t>(&back().value()) &&
                   m_actor.cluster().transitional())
        {
            // Here we can receive spurious commit, if previous commit was truncated from log due to leader change.
            m_actor.cluster().commit();
        }

        update_snapshot();
        m_actor.cluster().replicate();
    }

    void
    truncate(uint64_t index) {
        // Now we don't know if truncated entries will be committed,
        // so we provide corresponding error to their handlers.
        // Also we should rollback uncommitted configuration change, if we truncate it.
        for(auto i = last_index(); i >= index; --i) {
            auto &entry = m_log[index];

            if (m_actor.cluster().transitional() &&
                (boost::get<insert_node_t>(&entry.value()) ||
                 boost::get<erase_node_t>(&entry.value())))
            {
                m_actor.cluster().rollback();
            }

            entry.set_error(std::error_code(raft_errc::unknown));
        }
        m_log.truncate(index);
    }

    void
    reset_snapshot(uint64_t index,
                   uint64_t term,
                   snapshot_type&& snapshot)
    {
        m_log.set_snapshot(index, term, std::move(snapshot));
        m_actor.config().set_last_applied(index - 1);
        m_next_snapshot.reset();
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

private:
    void
    update_snapshot() {
        if(m_next_snapshot &&
           last_index() > m_next_snapshot_index + m_actor.options().snapshot_threshold / 2)
        {
            COCAINE_LOG_DEBUG(m_logger,
                              "Truncate the log up to %d index and save snapshot of the state machine.",
                              m_next_snapshot_index);

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

        template<class Event>
        typename command_traits<io::aux::frozen<Event>>::value_type
        operator()(const io::aux::frozen<Event>& command) const {
            return machine(command);
        }

        void
        operator()(const insert_node_t&) const {
            actor.template call<commit_node_t>(nullptr, commit_node_t());
        }

        void
        operator()(const erase_node_t&) const {
            actor.template call<commit_node_t>(nullptr, commit_node_t());
        }

        void
        operator()(const commit_node_t&) const {
            // Ignore.
        }

        void
        operator()(const nop_t&) const {
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
            COCAINE_LOG_DEBUG(m_logger, "Stop applier.");
            m_background_worker.stop();
            return;
        }

        COCAINE_LOG_DEBUG(m_logger,
                          "Applying entries from %d to %d.",
                          m_actor.config().last_applied() + 1,
                          to_apply);

        // Apply snapshot if it's not applied yet.
        // Entries will be applied on the next iterations, because we don't want occupy event loop for a long time.
        if(snapshot_index() > m_actor.config().last_applied()) {
            try {
                m_machine.consume(std::get<0>(m_log.snapshot()));
                m_actor.cluster().consume(std::get<1>(m_log.snapshot()));
            } catch(...) {
                return;
            }
            m_actor.config().set_last_applied(snapshot_index());

            return;
        }

        for(size_t index = m_actor.config().last_applied() + 1; index <= to_apply; ++index) {
            auto& entry = m_log[index];
            try {
                entry.visit(entry_visitor_t {m_machine, m_actor});
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
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_LOG_HANDLE_HPP
