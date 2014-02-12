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

#ifndef COCAINE_RAFT_REPOSITORY_IMPL_HPP
#define COCAINE_RAFT_REPOSITORY_IMPL_HPP

#include "cocaine/detail/raft/repository.hpp"

namespace cocaine { namespace raft {

template<class Machine, class Config>
std::shared_ptr<raft::actor<Machine, typename std::decay<Config>::type>>
repository_t::insert(const std::string& name, Machine&& machine, Config&& config) {
    auto actors = m_actors.synchronize();

    typedef typename std::decay<Machine>::type machine_type;
    typedef raft::actor<machine_type, typename std::decay<Config>::type> actor_type;

    raft::options_t opt = { m_context.config.raft.election_timeout,
                            m_context.config.raft.heartbeat_timeout,
                            m_context.config.raft.snapshot_threshold,
                            m_context.config.raft.message_size };

    auto actor = std::make_shared<actor_type>(m_context,
                                              *m_reactor,
                                              name,
                                              std::forward<Machine>(machine),
                                              std::forward<Config>(config),
                                              opt);

    if(actors->insert(std::make_pair(name, actor)).second) {
        m_reactor->post(std::bind(&actor_type::run, actor));
        return actor;
    } else {
        return std::shared_ptr<actor_type>();
    }
}

template<class Machine>
std::shared_ptr<raft::actor<Machine, raft::configuration<raft::log<Machine>>>>
repository_t::insert(const std::string& name, Machine&& machine) {
    auto actors = m_actors.synchronize();

    typedef typename std::decay<Machine>::type machine_type;
    typedef raft::actor<machine_type, raft::configuration<raft::log<machine_type>>> actor_type;

    raft::configuration<raft::log<machine_type>> config(
        std::make_pair(m_context.config.network.endpoint, m_context.config.network.locator),
        m_context.config.raft.cluster
    );

    raft::options_t opt = { m_context.config.raft.election_timeout,
                            m_context.config.raft.heartbeat_timeout,
                            m_context.config.raft.snapshot_threshold,
                            m_context.config.raft.message_size };

    auto actor = std::make_shared<actor_type>(m_context,
                                              *m_reactor,
                                              name,
                                              std::forward<Machine>(machine),
                                              std::move(config),
                                              opt);

    if(actors->insert(std::make_pair(name, actor)).second) {
        m_reactor->post(std::bind(&actor_type::run, actor));
        return actor;
    } else {
        return std::shared_ptr<actor_type>();
    }
}

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_REPOSITORY_HPP
