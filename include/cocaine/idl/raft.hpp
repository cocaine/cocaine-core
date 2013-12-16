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

#include "cocaine/rpc/protocol.hpp"

namespace cocaine { namespace io {

template<class Entry>
struct raft_tag;

template<class Entry>
struct raft {
    struct append {
        typedef raft_tag<Entry> tag;

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
            std::pair<std::string, uint16_t>,
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

    struct request_vote {
        typedef raft_tag<Entry> tag;

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
            std::pair<std::string, uint16_t>,
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

template<class Entry>
struct protocol<raft_tag<Entry>> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        raft<Entry>::append,
        raft<Entry>::request_vote
    > messages;

    typedef raft<Entry> type;
};

}} // namespace cocaine::io

#endif // COCAINE_RAFT_SERVICE_INTERFACE_HPP
