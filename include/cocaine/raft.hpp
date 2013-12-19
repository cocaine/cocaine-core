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

#ifndef COCAINE_RAFT_HPP
#define COCAINE_RAFT_HPP

#include "cocaine/common.hpp"
#include "cocaine/format.hpp"

namespace cocaine {

typedef std::pair<std::string, uint16_t> node_id_t;

namespace io {

struct raft_tag;

struct raft {

    struct append {
        typedef raft_tag tag;

        static
        const char*
        alias() {
            return "append";
        }

        typedef boost::mpl::list<
        /* Leader's term. */
            uint64_t,
        /* Leader's id. */
            node_id_t,
        /* Index and term of log entry immediately preceding new ones. (prev_log_index, prev_log_term) */
            std:tuple<uint64_t, uint64_t>,
        /* Entries to append. */
            std::vector<dynamic_t>,
        /* Leader's commit_index. */
            uint64_t
        > tuple_type;

        typedef stream_of<
        /* Term of the follower. */
            uint64_t,
        /* Success. */
            bool
        >::tag drain_type;
    };

    struct vote {
        typedef raft_tag tag;

        static
        const char*
        alias() {
            return "vote";
        }

        typedef boost::mpl::list<
        /* Candidate's term. */
            uint64_t,
        /* Candidate's id. */
            node_id_t,
        /* Index and term of candidate's last log entry. */
            std:tuple<uint64_t, uint64_t>
        > tuple_type;

        typedef stream_of<
        /* Term of the follower. */
            uint64_t,
        /* Vote granted. */
            bool
        >::tag drain_type;
    };

}; // struct raft

template<>
struct protocol<raft_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        raft::append,
        raft::vote
    > messages;

    typedef raft type;
};

} // namespace io

struct state_machine_concept {
    virtual
    void
    operator()(const dynamic_t&) = 0;
};

struct remote_node_t {
    std::shared_ptr<io::channel<io::socket<io::tcp>>> channel;

    // The next log entry to send to the follower.
    uint64_t next_index;

    // The last entry replicated to the follower.
    uint64_t match_index;
};

struct log_entry_t {
    uint64_t term;
    dynamic_t data;
};

class raft_actor_t :
    public implements<io::raft_tag>
{
    raft_actor_t(context_t& context, const std::string& name) :
        implements<io::raft_tag>(context, name)
    {
        using namespace std::placeholders;

        on<io::raft::append>(std::bind(&raft_actor_t::on_append, this, _1, _2, _3, _4, _5));
        on<io::raft::vote>(std::bind(&raft_actor_t::on_vote, this, _1, _2, _3));
    }

    std::tuple<uint64_t, bool>
    on_append(uint64_t term,
              node_id_t leader,
              std:tuple<uint64_t, uint64_t> prev_entry, // index, term
              std::vector<dynamic_t> entries,
              uint64_t commit_index);

    std::tuple<uint64_t, bool>
    on_vote(uint64_t term,
            node_id_t candidate,
            std:tuple<uint64_t, uint64_t> last_entry);

    void
    switch_to_candidate();

    void
    switch_to_leader();

    void
    switch_to_follower();

private:
    node_id_t m_id;
    std::shared_ptr<state_machine_concept> m_state_machine;

    std::map<node_id_t, remote_node_t> m_cluster;

    uint64_t m_current_term;
    std::vector<log_entry_t> m_log;

    // The highest index known to be commited.
    uint64_t m_commit_index;

    // The last entry applied to the state machine.
    uint64_t m_last_applied;

    node_id_t m_voted_for;

    // The leader from the last append message.
    node_id_t m_leader;

    // When the timer fires, the follower will switch to the candidate state.
    ev::timer m_disown_timer;
};

} // namespace cocaine

#endif // COCAINE_RAFT_HPP
