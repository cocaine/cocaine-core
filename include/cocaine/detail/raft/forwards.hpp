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

#ifndef COCAINE_RAFT_FORWARD_DECLARATIONS_HPP
#define COCAINE_RAFT_FORWARD_DECLARATIONS_HPP

#include "cocaine/detail/raft/options.hpp"
#include "cocaine/detail/raft/entry.hpp"

#include "cocaine/rpc/slots/deferred.hpp"

#include <set>
#include <string>
#include <utility>

namespace cocaine { namespace raft {

// Identifier of RAFT node. In fact this is endpoint of locator of the node.
typedef std::pair<std::string, uint16_t> node_id_t;

// NOTE: May be removed in the future.
typedef std::set<node_id_t> cluster_t;

// Concept of RAFT actor.
class actor_concept_t {
public:
    virtual
    deferred<std::tuple<uint64_t, bool>>
    append(uint64_t term,
           node_id_t leader,
           std::tuple<uint64_t, uint64_t> prev_entry, // index, term
           const std::vector<msgpack::object>& entries,
           uint64_t commit_index) = 0;

    virtual
    deferred<std::tuple<uint64_t, bool>>
    apply(uint64_t term,
          node_id_t leader,
          std::tuple<uint64_t, uint64_t> prev_entry, // index, term
          const msgpack::object& snapshot,
          uint64_t commit_index) = 0;

    virtual
    deferred<std::tuple<uint64_t, bool>>
    request_vote(uint64_t term,
                 node_id_t candidate,
                 std::tuple<uint64_t, uint64_t> last_entry) = 0;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_FORWARD_DECLARATIONS_HPP
