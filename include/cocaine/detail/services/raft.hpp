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
#include "cocaine/dispatch.hpp"
#include "cocaine/api/service.hpp"
#include "cocaine/idl/raft.hpp"
#include "cocaine/detail/raft/log.hpp"

namespace cocaine {

namespace raft {
    typedef std::pair<std::string, uint16_t> node_id_t;

    class actor_concept_t;

    template<class Dispatch, class Log>
    class actor;
}

class raft_service_t:
    public api::service_t,
    public implements<io::raft_tag<msgpack::object>>
{
public:
    raft_service_t(context_t& context,
                   io::reactor_t& reactor,
                   const std::string& name,
                   const dynamic_t& args);

    const raft::node_id_t&
    id() const;

    const std::set<raft::node_id_t>&
    cluster() const;

    context_t&
    context();

    io::reactor_t&
    reactor();

    uint64_t
    election_timeout() const;

    uint64_t
    heartbeat_timeout() const;

    template<class Machine, class Log>
    std::shared_ptr<raft::actor<Machine, typename std::decay<Log>::type>>
    add(const std::string& name,
        const std::shared_ptr<Machine>& machine,
        Log&& log);

    template<class Machine>
    std::shared_ptr<raft::actor<Machine, raft::log<Machine>>>
    add(const std::string& name, const std::shared_ptr<Machine>& machine);


private:
    std::tuple<uint64_t, bool>
    append(const std::string& state_machine,
           uint64_t term,
           raft::node_id_t leader,
           std::tuple<uint64_t, uint64_t> prev_entry, // index, term
           const std::vector<msgpack::object>& entries,
           uint64_t commit_index);

    std::tuple<uint64_t, bool>
    request_vote(const std::string& state_machine,
                 uint64_t term,
                 raft::node_id_t candidate,
                 std::tuple<uint64_t, uint64_t> last_entry);

private:
    raft::node_id_t m_self;
    std::set<raft::node_id_t> m_cluster;
    context_t& m_context;
    io::reactor_t& m_reactor;
    uint64_t m_election_timeout;
    uint64_t m_heartbeat_timeout;

    synchronized<std::map<std::string, std::shared_ptr<raft::actor_concept_t>>> m_actors;
};

} // namespace cocaine

#include "cocaine/detail/raft/actor.hpp"

namespace cocaine {

template<class Machine, class Log>
std::shared_ptr<raft::actor<Machine, typename std::decay<Log>::type>>
raft_service_t::add(const std::string& name,
                    const std::shared_ptr<Machine>& machine,
                    Log&& log)
{
    auto actors = m_actors.synchronize();

    auto actor = std::make_shared<raft::actor<Machine, typename std::decay<Log>::type>>(
        *this,
        name,
        machine,
        std::forward<Log>(log)
    );

    if(actors->insert(std::make_pair(name, actor)).second) {
        return actor;
    } else {
        return std::shared_ptr<raft::actor<Machine, typename std::decay<Log>::type>>();
    }
}

template<class Machine>
std::shared_ptr<raft::actor<Machine, raft::log<Machine>>>
raft_service_t::add(const std::string& name, const std::shared_ptr<Machine>& machine) {
    auto actors = m_actors.synchronize();

    auto actor = std::make_shared<raft::actor<Machine, raft::log<Machine>>>(
        *this,
        name,
        machine,
        raft::log<Machine>()
    );

    if(actors->insert(std::make_pair(name, actor)).second) {
        return actor;
    } else {
        return std::shared_ptr<raft::actor<Machine, raft::log<Machine>>>();
    }
}

} // namespace cocaine

#endif // COCAINE_RAFT_SERVICE_HPP
