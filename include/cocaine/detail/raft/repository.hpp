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

#include "cocaine/common.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/detail/raft/actor.hpp"
#include "cocaine/detail/raft/configuration.hpp"
#include "cocaine/detail/raft/log.hpp"

namespace cocaine { namespace raft {

class repository_t {
    friend class cocaine::context_t;

public:
    repository_t(context_t& context):
        m_context(context),
        m_reactor(std::make_shared<io::reactor_t>())
    { }

    const std::shared_ptr<raft::actor_concept_t>&
    get(const std::string& name) const;

    template<class Machine, class Config>
    std::shared_ptr<raft::actor<Machine, typename std::decay<Config>::type>>
    insert(const std::string& name, Machine&& machine, Config&& config);

    template<class Machine>
    std::shared_ptr<raft::actor<Machine, raft::configuration<raft::log<Machine>>>>
    insert(const std::string& name, Machine&& machine);

private:
    context_t& m_context;
    std::shared_ptr<io::reactor_t> m_reactor;

    synchronized<std::map<std::string, std::shared_ptr<raft::actor_concept_t>>> m_actors;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_REPOSITORY_HPP
