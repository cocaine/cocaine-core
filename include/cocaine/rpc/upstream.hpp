/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_IO_UPSTREAM_HPP
#define COCAINE_IO_UPSTREAM_HPP

#include "cocaine/rpc/session.hpp"

namespace cocaine {

template<class Tag> class upstream;

namespace io {

class basic_upstream_t {
    const std::shared_ptr<session_t> session;
    const uint64_t channel_id;

    enum class states { active, sealed } state;

public:
    basic_upstream_t(const std::shared_ptr<session_t>& session_, uint64_t channel_id_):
        session(session_),
        channel_id(channel_id_)
    {
        state = states::active;
    }

    template<class Event, typename... Args>
    void
    send(Args&&... args);

    void
    drop();
};

template<class Event, typename... Args>
void
basic_upstream_t::send(Args&&... args) {
    if(state != states::active) {
        return;
    }

    if(std::is_same<typename io::event_traits<Event>::dispatch_type, void>::value) {
        // NOTE: Sealed upstreams ignore any messages. At some point it might change to an explicit
        // way to show that the operation won't be completed.
        state = states::sealed;
    }

    session->push(encoded<Event>(channel_id, std::forward<Args>(args)...));
}

inline
void
basic_upstream_t::drop() {
    session->revoke(channel_id);
}

// Forwards for the upstream<T> class

template<class Tag, class Upstream> class message_queue;

} // namespace io

template<class Tag>
class upstream {
    template<class, class> friend class io::message_queue;

    // The original non-typed upstream.
    const io::upstream_ptr_t ptr;

public:
    upstream(const io::upstream_ptr_t& upstream_):
        ptr(upstream_)
    { }

    template<class Event, typename... Args>
    void
    send(Args&&... args) {
        static_assert(
            std::is_same<typename Event::tag, Tag>::value,
            "message protocol is not compatible with this upstream"
        );

        ptr->send<Event>(std::forward<Args>(args)...);
    }
};

template<>
class upstream<void> {
    template<class, class> friend class io::message_queue;

public:
    upstream(const io::upstream_ptr_t& COCAINE_UNUSED_(upstream)) {
        // Empty.
    }
};

} // namespace cocaine

#endif
