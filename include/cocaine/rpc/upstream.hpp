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
    const uint64_t channel;

    enum class states { active, sealed } state;

public:
    basic_upstream_t(const std::shared_ptr<session_t>& session_, uint64_t channel_):
        session(session_),
        channel(channel_)
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

    session->push(encoded<Event>(channel, std::forward<Args>(args)...));
}

inline
void
basic_upstream_t::drop() {
    session->revoke(channel);
}

// Forwards for the upstream<T> class

template<class Tag> class message_queue;

} // namespace io

template<class Tag>
class upstream {
    // It should be constant, but GCC 4.4 can't compile C++.
    std::shared_ptr<io::basic_upstream_t> ptr;

public:
    template<class> friend class io::message_queue;

    explicit
    upstream(const std::shared_ptr<io::basic_upstream_t>& upstream_):
        ptr(upstream_)
    { }

    // Protocol constraint for this upstream.
    typedef typename io::protocol<Tag>::scope protocol;

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

} // namespace cocaine

#endif
