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

#include "cocaine/rpc/queue.hpp"

#include <boost/optional.hpp>

#include <set>
#include <string>
#include <utility>
#include <functional>

namespace cocaine { namespace raft {

// Identifier of RAFT node. In fact this is endpoint of locator of the node.
typedef std::pair<std::string, uint16_t> node_id_t;

// May be removed in the future.
typedef std::set<node_id_t> cluster_t;

// RAFT actor state.
enum class state_t {
    leader,
    candidate,
    follower
};

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
    request_vote(uint64_t term,
                 node_id_t candidate,
                 std::tuple<uint64_t, uint64_t> last_entry) = 0;
};

// Special type of log entry, which does nothing.
struct nop_t {
    bool
    operator==(const nop_t&) const {
        return true;
    }
};

template<class StateMachine>
class log_entry {
    typedef StateMachine machine_type;

    typedef typename boost::mpl::transform<
        typename io::protocol<typename machine_type::tag>::messages,
        typename boost::mpl::lambda<io::aux::frozen<boost::mpl::arg<1>>>
    >::type wrapped_type;

public:
    typedef typename boost::make_variant_over<wrapped_type>::type command_type;
    typedef typename boost::variant<nop_t, command_type> value_type;

public:
    log_entry():
        m_term(0),
        m_value(nop_t())
    { }

    log_entry(uint64_t term, const value_type& value):
        m_term(term),
        m_value(value)
    { }

    uint64_t
    term() const {
        return m_term;
    }

    value_type&
    value() {
        return m_value;
    }

    const value_type&
    value() const {
        return m_value;
    }

    bool
    is_command() const {
        return boost::get<command_type>(&m_value);
    }

    const command_type&
    get_command() const {
        return boost::get<command_type>(m_value);
    }

    command_type&
    get_command() {
        return boost::get<command_type>(m_value);
    }

    template<class Handler>
    void
    bind(Handler&& h) {
        m_commit_handler = h;
    }

    void
    notify(boost::optional<uint64_t> index) {
        if(m_commit_handler) {
            m_commit_handler(index);
        }
    }

private:
    uint64_t m_term;
    value_type m_value;
    std::function<void(boost::optional<uint64_t>)> m_commit_handler;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_FORWARD_DECLARATIONS_HPP
