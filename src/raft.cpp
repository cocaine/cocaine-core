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

#include "cocaine/detail/raft/service.hpp"
#include "cocaine/raft.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::raft;

std::error_code
cocaine::make_error_code(raft_errc e) {
    return std::error_code(static_cast<int>(e), raft_category());
}

std::error_condition
cocaine::make_error_condition(raft_errc e) {
    return std::error_condition(static_cast<int>(e), raft_category());
}

const std::shared_ptr<raft::actor_concept_t>&
raft::repository_t::get(const std::string& name) const {
    auto actors = m_actors.synchronize();

    auto it = actors->find(name);

    if(it != actors->end()) {
        return it->second;
    } else {
        throw error_t("There is no such state machine.");
    }
}

service_t::service_t(context_t& context,
                     io::reactor_t& reactor,
                     const std::string& name):
    api::service_t(context, reactor, name, dynamic_t::empty_object),
    implements<io::raft_tag<msgpack::object, msgpack::object>>(context, name),
    m_context(context),
    m_reactor(reactor)
{
    using namespace std::placeholders;

    on<io::raft<msgpack::object, msgpack::object>::append>(std::bind(&service_t::append, this, _1, _2, _3, _4, _5, _6));
    on<io::raft<msgpack::object, msgpack::object>::apply>(std::bind(&service_t::apply, this, _1, _2, _3, _4, _5, _6));
    on<io::raft<msgpack::object, msgpack::object>::request_vote>(std::bind(&service_t::request_vote, this, _1, _2, _3, _4));
}

deferred<std::tuple<uint64_t, bool>>
service_t::append(const std::string& machine,
                  uint64_t term,
                  node_id_t leader,
                  std::tuple<uint64_t, uint64_t> prev_entry, // index, term
                  const std::vector<msgpack::object>& entries,
                  uint64_t commit_index)
{
    return m_context.raft->get(machine)->append(term, leader, prev_entry, entries, commit_index);
}

deferred<std::tuple<uint64_t, bool>>
service_t::apply(const std::string& machine,
                 uint64_t term,
                 raft::node_id_t leader,
                 std::tuple<uint64_t, uint64_t> snapshot_entry, // index, term
                 const msgpack::object& snapshot,
                 uint64_t commit_index)
{
    return m_context.raft->get(machine)->apply(term, leader, snapshot_entry, snapshot, commit_index);
}

deferred<std::tuple<uint64_t, bool>>
service_t::request_vote(const std::string& state_machine,
                        uint64_t term,
                        raft::node_id_t candidate,
                        std::tuple<uint64_t, uint64_t> last_entry)
{
    return m_context.raft->get(state_machine)->request_vote(term, candidate, last_entry);
}
