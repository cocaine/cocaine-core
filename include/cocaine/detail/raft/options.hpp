/*
    Copyright (c) 2014-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_RAFT_ACTOR_OPTIONS_HPP
#define COCAINE_RAFT_ACTOR_OPTIONS_HPP

namespace cocaine { namespace raft {

// Some arguments of Raft actor.
struct options_t {
    unsigned int election_timeout;

    // This timeout shouldn't be more than election_timeout. Optimal value is (election_timeout / 2).
    unsigned int heartbeat_timeout;

    // Actor will truncate the log and save snapshot of the state machine every snapshot_threshold committed entries.
    unsigned int snapshot_threshold;

    // Leader will send at most message_size entries in one append message.
    // Also actor will apply at most message_size entries at once.
    unsigned int message_size;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_ACTOR_OPTIONS_HPP
