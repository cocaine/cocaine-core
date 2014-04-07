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

#ifndef COCAINE_RAFT_REPOSITORY_HPP
#define COCAINE_RAFT_REPOSITORY_HPP

#include "cocaine/detail/raft/configuration.hpp"
#include "cocaine/detail/raft/log.hpp"

#include "cocaine/context.hpp"
#include "cocaine/locked_ptr.hpp"

namespace cocaine { namespace raft {

// This class stores Raft actors, which replicate named state machines.
// Raft service uses this class to deliver messages from other nodes to actors.
// Core uses it to setup new state machines.
class repository_t {
    friend class cocaine::context_t;
    friend class configuration_machine_t;

public:
    typedef std::map<std::string, lockable_config_t> configs_type;

    repository_t(context_t& context);

    const node_id_t&
    id() const {
        return m_id;
    }

    locked_ptr<const configs_type>
    configuration() const {
        return m_configs.synchronize();
    }

    std::shared_ptr<actor_concept_t>
    get(const std::string& name) const;

    template<class Machine, class Config>
    std::shared_ptr<actor<
        typename std::decay<Machine>::type,
        typename std::decay<Config>::type
    >>
    insert(const std::string& name, Machine&& machine, Config&& config);

    template<class Machine>
    std::shared_ptr<actor<
        typename std::decay<Machine>::type,
        cocaine::raft::configuration<typename std::decay<Machine>::type>
    >>
    insert(const std::string& name, Machine&& machine);

private:
    context_t& m_context;

    std::shared_ptr<io::reactor_t> m_reactor;

    node_id_t m_id;

    synchronized<std::map<std::string, std::shared_ptr<actor_concept_t>>> m_actors;

    synchronized<configs_type> m_configs;
};

}} // namespace cocaine::raft

#include "cocaine/detail/raft/actor.hpp"

namespace cocaine { namespace raft {

template<class Machine, class Config>
std::shared_ptr<actor<
    typename std::decay<Machine>::type,
    typename std::decay<Config>::type
>>
repository_t::insert(const std::string& name, Machine&& machine, Config&& config) {
    auto actors = m_actors.synchronize();

    typedef typename std::decay<Machine>::type machine_type;
    typedef typename std::decay<Config>::type config_type;
    typedef actor<machine_type, config_type> actor_type;

    options_t opt = { m_context.config.raft.election_timeout,
                      m_context.config.raft.heartbeat_timeout,
                      m_context.config.raft.snapshot_threshold,
                      m_context.config.raft.message_size,
                      m_context.config.raft.some_nodes };

    auto actor = std::make_shared<actor_type>(m_context,
                                              *m_reactor,
                                              name,
                                              std::forward<Machine>(machine),
                                              std::forward<Config>(config),
                                              opt);

    if(actors->insert(std::make_pair(name, actor)).second) {
        if(name == m_context.config.raft.config_machine_name &&
           m_context.config.raft.create_configuration_cluster)
        {
            m_reactor->post(std::bind(&actor_type::create_cluster, actor));
        } else {
            m_reactor->post(std::bind(&actor_type::join_cluster, actor));
        }
        return actor;
    } else {
        return std::shared_ptr<actor_type>();
    }
}

template<class Machine>
std::shared_ptr<actor<
    typename std::decay<Machine>::type,
    cocaine::raft::configuration<typename std::decay<Machine>::type>
>>
repository_t::insert(const std::string& name, Machine&& machine) {
    auto actors = m_actors.synchronize();

    typedef typename std::decay<Machine>::type machine_type;
    typedef cocaine::raft::configuration<typename std::decay<Machine>::type> config_type;
    typedef actor<machine_type, config_type> actor_type;

    config_type config(
        id(),
        cluster_config_t {std::set<node_id_t>(), boost::none}
    );

    options_t opt = { m_context.config.raft.election_timeout,
                      m_context.config.raft.heartbeat_timeout,
                      m_context.config.raft.snapshot_threshold,
                      m_context.config.raft.message_size,
                      m_context.config.raft.some_nodes };

    auto actor = std::make_shared<actor_type>(m_context,
                                              *m_reactor,
                                              name,
                                              std::forward<Machine>(machine),
                                              std::move(config),
                                              opt);

    if(actors->insert(std::make_pair(name, actor)).second) {
        if(name == m_context.config.raft.config_machine_name &&
           m_context.config.raft.create_configuration_cluster)
        {
            m_reactor->post(std::bind(&actor_type::create_cluster, actor));
        } else {
            m_reactor->post(std::bind(&actor_type::join_cluster, actor));
        }
        return actor;
    } else {
        return std::shared_ptr<actor_type>();
    }
}

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_REPOSITORY_HPP
