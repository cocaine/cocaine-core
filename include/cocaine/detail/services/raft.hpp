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

#ifndef COCAINE_RAFT_SERVICE_HPP
#define COCAINE_RAFT_SERVICE_HPP

#include "cocaine/common.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/raft/actor.hpp"

namespace cocaine {

class raft_service_t:
    public api::service_t,
    public implements<io::raft_tag>
{
public:
    raft_service_t(context_t& context,
                   io::reactor_t& reactor,
                   const std::string& name,
                   const dynamic_t& args) :
        m_context(context),
        m_reactor(reactor)
    {
        m_actors = args.as_object().at("cluster", dynamic_t::empty_object).to<std::set<raft::node_id_t>>();
    }

    const std::set<raft::node_id_t>&
    cluster() const {
        return m_cluster;
    }

    context_t&
    context() {
        return m_context;
    }

    io::reactor_t&
    reactor() {
        return m_reactor;
    }

    template<class Actor>
    std::shared_ptr<Actor>
    add(const std::string& name, const std::shared_ptr<typename Actor::machine_type>& machine) {
        auto actors = m_actors.synchronize();

        auto actor = std::make_shared<Actor>(*this, machine);

        if(actors->insert(std::make_pair(name, actor)).second) {
            return actor;
        } else {
            return std::shared_ptr<Actor>();
        }
    }


private:
    std::tuple<uint64_t, bool>
    append(const std::string& state_mahine,
           uint64_t term,
           io::raft::node_id_t leader,
           std:tuple<uint64_t, uint64_t> prev_entry, // index, term
           const std::vector<msgpack::object>& entries,
           uint64_t commit_index)
    {
        auto actors = m_actors.synchronize();

        auto it = actors->find(state_machine);

        if(it != actors->end()) {
            return it->second->append(term, leader, prev_entry, entries, commit_index);
        } else {
            throw error_t("There is no such state machine.");
        }
    }

    std::tuple<uint64_t, bool>
    request_vote(const std::string& state_mahine,
                 uint64_t term,
                 io::raft::node_id_t candidate,
                 std:tuple<uint64_t, uint64_t> last_entry)
    {
        auto actors = m_actors.synchronize();

        auto it = actors->find(state_machine);

        if(it != actors->end()) {
            return it->second->request_vote(term, candidate, last_entry);
        } else {
            throw error_t("There is no such state machine.");
        }
    }

private:
    std::set<raft::node_id_t> m_cluster;
    context_t& m_context;
    io::reactor_t& m_reactor;

    synchronized<std::map<std::string, std::shared_ptr<raft::actor_concept_t>>> m_actors;
};

} // namespace cocaine

#endif // COCAINE_RAFT_SERVICE_HPP
