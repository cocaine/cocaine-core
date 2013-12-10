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
#include "cocaine/format.hpp"

namespace cocaine {

typedef std::pair<std::string, uint16_t> node_id_t;

struct log_entry_t {
    enum type_t {
        command,
        configuration,
        snapshot,
        nop
    };

    uint64_t term;
    type_t type;
    dynamic_t data;
};

class remote_node_t {
public:
    remote_node_t(raft_actor_t& local, node_id_t id);

    void
    request_vote(const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>&,
                 uint64_t term,
                 node_id_t candidate,
                 std:tuple<uint64_t, uint64_t> last_entry);

    void
    replicate(const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>&,
              uint64_t term,
              node_id_t candidate,
              std:tuple<uint64_t, uint64_t> last_entry,
              const std::vector<log_entry_t>& entries,
              uint64_t commit_index);

    uint64_t
    next_index() const {
        return m_next_index;
    }

    uint64_t
    match_index() const {
        return m_match_index;
    }

    void
    reset();

private:
    void
    ensure_connection();

    void
    on_error(const std::error_code& ec);

private:
    node_id_t m_id;

    raft_actor_t &m_local;

    std::shared_ptr<client_t> m_client;

    // The next log entry to send to the follower.
    uint64_t m_next_index;

    // The last entry replicated to the follower.
    uint64_t m_match_index;
};

struct raft_actor_concept {
    virtual
    std::tuple<uint64_t, bool>
    on_append(uint64_t term,
              io::raft::node_id_t leader,
              std:tuple<uint64_t, uint64_t> prev_entry, // index, term
              const std::vector<msgpack::object>& entries,
              uint64_t commit_index) = 0;

    virtual
    std::tuple<uint64_t, bool>
    on_vote(uint64_t term,
            io::raft::node_id_t candidate,
            std:tuple<uint64_t, uint64_t> last_entry) = 0;
};

template<class Tag>
class log_entry {
public:
    typedef typename boost::mpl::transform<
        typename io::protocol<Tag>::messages,
        typename boost::mpl::lambda<io::aux::frozen<boost::mpl::_1>>
    >::type frozen_types;

    typedef typename boost::make_variant_over<frozen_types>::type variant_type;

    enum type_t {
        command,
        configuration,
        snapshot,
        nop
    };

public:
    log_entry(uint64_t term, const variant_type& data);

    log_entry(uint64_t term);

    uint64_t
    term() const;

private:
    uint64_t m_term;
    type_t m_type;
    variant_type m_data;
};

template<class Tag>
class log {
public:

private:

};

template<class Dispatch, class Log = log<typename Dispatch::tag>>
class raft_actor:
    public raft_actor_concept
{
    typedef Dispatch machine_type;

    typedef Log log_type;

    raft_actor(context_t& context,
               io::reactor_t& reactor,
               const std::string& name,
               const std::shared_ptr<machine_type>& state_machine);

    context_t&
    context() {
        return m_context;
    }

    io::reactor_t&
    reactor() const {
        return m_reactor;
    }

    std::tuple<uint64_t, bool>
    on_append(uint64_t term,
              io::raft::node_id_t leader,
              std:tuple<uint64_t, uint64_t> prev_entry, // index, term
              const std::vector<msgpack::object>& entries,
              uint64_t commit_index);

    std::tuple<uint64_t, bool>
    on_vote(uint64_t term,
            io::raft::node_id_t candidate,
            std:tuple<uint64_t, uint64_t> last_entry);

private:
    void
    append(const log_entry_t& entry);

    void
    start_election();

    void
    switch_to_leader();

    void
    reset_election_state();

private:
    raft_service_t &m_service;

    std::map<io::raft::node_id_t, remote_node_t> m_cluster;

    std::shared_ptr<machine_type> m_state_machine;

    log_type m_log;

    uint64_t m_current_term;

    // The highest index known to be commited.
    uint64_t m_commit_index;

    // The last entry applied to the state machine.
    uint64_t m_last_applied;

    boost::option<node_id_t> m_voted_for;

    // The leader from the last append message.
    node_id_t m_leader;

    // When the timer fires, the follower will switch to the candidate state.
    ev::timer m_election_timer;

    // When the timer fires, the follower will switch to the candidate state.
    ev::timer m_heartbeat_timer;

    // This watcher will apply committed entries in background.
    ev::idle m_applier;

    // This state will contain information about received votes and will be reset at finish of the election.
    class election_state_t;

    std::shared_ptr<election_state_t> m_election_state;
};

class raft_service_t:
    public api::service_t,
    public implements<io::raft_tag>
{
public:

private:
    std::map<std::string, std::shared_ptr<raft_actor_concept>> m_actors;
};

} // namespace cocaine

#endif // COCAINE_RAFT_SERVICE_HPP
