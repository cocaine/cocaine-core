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

#ifndef COCAINE_RAFT_CONFIG_HANDLE_HPP
#define COCAINE_RAFT_CONFIG_HANDLE_HPP

#include "cocaine/detail/raft/forwards.hpp"

#include "cocaine/logging.hpp"

namespace cocaine { namespace raft {

// This class is wrapper around configuration. It's needed to notify other components of the Raft
// implementation about configuration changes.
template<class Actor>
class config_handle {
    COCAINE_DECLARE_NONCOPYABLE(config_handle)

    typedef Actor actor_type;
    typedef typename actor_type::config_type config_type;
    typedef typename config_type::cluster_type cluster_type;

public:
    config_handle(actor_type &actor, config_type &config):
        m_actor(actor),
        m_logger(new logging::log_t(actor.context(), "raft/" + actor.name())),
        m_config(config)
    { }

    const node_id_t&
    id() const {
        return m_config.id();
    }

    cluster_type&
    cluster() {
        return m_config.cluster();
    }

    const cluster_type&
    cluster() const {
        return m_config.cluster();
    }

    uint64_t
    current_term() const {
        return m_config.current_term();
    }

    void
    set_current_term(uint64_t value) {
        m_config.set_commit_index(value);
    }

    uint64_t
    commit_index() const {
        return m_config.commit_index();
    }

    // Update commit index to new value and start applier, if it's needed.
    void
    set_commit_index(uint64_t value) {
        auto new_index = std::min(value, m_actor.log().last_index());
        m_config.set_commit_index(value);

        COCAINE_LOG_DEBUG(m_logger, "Commit index has been updated to %d.", new_index);

        m_actor.log().apply();
    }

    uint64_t
    last_applied() const {
        return m_config.last_applied();
    }

    void
    set_last_applied(uint64_t value) {
        m_config.set_last_applied(value);
    }

    void
    increase_last_applied() {
        set_last_applied(last_applied() + 1);
    }

private:
    actor_type &m_actor;

    const std::unique_ptr<logging::log_t> m_logger;

    config_type &m_config;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_CONFIG_HANDLE_HPP
