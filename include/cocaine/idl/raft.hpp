/*
    Copyright (c) 2011-2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_RAFT_SERVICE_INTERFACE_HPP
#define COCAINE_RAFT_SERVICE_INTERFACE_HPP

#include "cocaine/detail/raft/forwards.hpp"

#include "cocaine/rpc/protocol.hpp"

namespace cocaine { namespace io {

template<class Entry, class Snapshot>
struct raft_node_tag;

template<class Entry, class Snapshot>
struct raft_node {

// Append new entries to the log. Leader sends this message to followers to replicate state machine.
struct append {
    typedef raft_node_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "append";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string,
     /* Leader's term. */
        uint64_t,
     /* Leader's id. */
        cocaine::raft::node_id_t,
     /* Index and term of log entry immediately preceding new ones. (prev_log_index, prev_log_term) */
        std::tuple<uint64_t, uint64_t>,
     /* Entries to append. */
        std::vector<Entry>,
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

// Store snapshot of state machine on the follower.
// When follower is far behind leader, leader firstly sends snapshot to the follower and then new entries.
struct apply {
    typedef raft_node_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "apply";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string,
     /* Leader's term. */
        uint64_t,
     /* Leader's id. */
        cocaine::raft::node_id_t,
     /* Index and term of the last log entry participating in the snapshot. */
        std::tuple<uint64_t, uint64_t>,
     /* Snapshot. */
        Snapshot,
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

struct request_vote {
    typedef raft_node_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "request_vote";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string,
     /* Candidate's term. */
        uint64_t,
     /* Candidate's id. */
        cocaine::raft::node_id_t,
     /* Index and term of candidate's last log entry. */
        std::tuple<uint64_t, uint64_t>
    > tuple_type;

    typedef stream_of<
     /* Term of the follower. */
        uint64_t,
     /* Vote granted. */
        bool
    >::tag drain_type;
};

struct insert {
    typedef raft_node_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "insert";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string,
     /* Node. */
        cocaine::raft::node_id_t
    > tuple_type;

    typedef stream_of<
        cocaine::raft::command_result<void>
    >::tag drain_type;
};

struct erase {
    typedef raft_node_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "erase";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string,
     /* Node. */
        cocaine::raft::node_id_t
    > tuple_type;

    typedef stream_of<
        cocaine::raft::command_result<void>
    >::tag drain_type;
};

}; // struct raft_node

template<class Entry, class Snapshot>
struct protocol<raft_node_tag<Entry, Snapshot>> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        typename raft_node<Entry, Snapshot>::append,
        typename raft_node<Entry, Snapshot>::apply,
        typename raft_node<Entry, Snapshot>::request_vote,
        typename raft_node<Entry, Snapshot>::insert,
        typename raft_node<Entry, Snapshot>::erase
    > messages;

    typedef raft_node<Entry, Snapshot> type;
};

template<class Entry, class Snapshot>
struct raft_control_tag;

template<class Entry, class Snapshot>
struct raft_control {

// Add node to state machine.
struct insert {
    typedef raft_control_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "insert";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string,
     /* Node. */
        cocaine::raft::node_id_t
    > tuple_type;

    typedef stream_of<
        cocaine::raft::command_result<cocaine::raft::cluster_change_result>
    >::tag drain_type;
};

struct erase {
    typedef raft_control_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "erase";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string,
     /* Node. */
        cocaine::raft::node_id_t
    > tuple_type;

    typedef stream_of<
        cocaine::raft::command_result<cocaine::raft::cluster_change_result>
    >::tag drain_type;
};

struct lock {
    typedef raft_control_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "lock";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string
    > tuple_type;

    typedef stream_of<
        cocaine::raft::command_result<void>
    >::tag drain_type;
};

struct reset {
    typedef raft_control_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "reset";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string,
     /* New value of the configuration. */
        cocaine::raft::cluster_config_t
    > tuple_type;

    typedef stream_of<
        cocaine::raft::command_result<void>
    >::tag drain_type;
};

struct dump {
    typedef raft_control_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "dump";
    }

    typedef stream_of<
        std::map<std::string, cocaine::raft::lockable_config_t>
    >::tag drain_type;
};

struct leader {
    typedef raft_control_tag<Entry, Snapshot> tag;

    static
    const char*
    alias() {
        return "leader";
    }

    typedef boost::mpl::list<
     /* Name of state machine. */
        std::string
    > tuple_type;

    typedef stream_of<
        cocaine::raft::node_id_t
    >::tag drain_type;
};

}; // struct raft_control

template<class Entry, class Snapshot>
struct protocol<raft_control_tag<Entry, Snapshot>> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        typename raft_control<Entry, Snapshot>::insert,
        typename raft_control<Entry, Snapshot>::erase,
        typename raft_control<Entry, Snapshot>::lock,
        typename raft_control<Entry, Snapshot>::reset,
        typename raft_control<Entry, Snapshot>::dump,
        typename raft_control<Entry, Snapshot>::leader
    > messages;

    typedef raft_control<Entry, Snapshot> type;
};

}} // namespace cocaine::io

#endif
