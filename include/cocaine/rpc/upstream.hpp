/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_IO_UPSTREAM_HPP
#define COCAINE_IO_UPSTREAM_HPP

#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/tcp.hpp"

#include "cocaine/rpc/session.hpp"
#include "cocaine/rpc/channel.hpp"

namespace cocaine {

class upstream_t {
    struct states {
        enum values: int { active, sealed };
    };

    // NOTE: Sealed streams ignore any messages. At some point it might change to an explicit way
    // to show that the operation won't be completed.
    states::values state;

    const std::shared_ptr<session_t> session;
    const uint64_t index;

public:
    upstream_t(const std::shared_ptr<session_t>& session_, uint64_t index_):
        state(states::active),
        session(session_),
        index(index_)
    { }

    template<class Event, typename... Args>
    void
    send(Args&&... args);
};

template<class Event, typename... Args>
void
upstream_t::send(Args&&... args) {
    std::lock_guard<std::mutex> guard(session->mutex);

    if(state != states::active) {
        return;
    }

    if(std::is_same<typename io::event_traits<Event>::transition_type, void>::value) {
        state = states::sealed;

        // If the event transition type is void, i.e. the remote dispatch will be destroyed after
        // receiving this message, then detach the stream with the given index in the channel, so
        // that new requests might reuse it in the future. This stream will become sealed.
        session->detach(index);
    }

    if(session->ptr) {
        session->ptr->wr->write<Event>(index, std::forward<Args>(args)...);
    }
}

} // namespace cocaine

#endif
