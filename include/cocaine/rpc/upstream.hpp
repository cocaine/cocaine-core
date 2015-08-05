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

public:
    basic_upstream_t(const std::shared_ptr<session_t>& session_, uint64_t channel_id_, boost::optional<trace_t> client_trace_):
        session(session_),
        channel_id(channel_id_),
        client_trace(client_trace_)
    { }

    template<class Event, class... Args>
    void
    send(Args&&... args);

    boost::optional<trace_t> client_trace;
};

template<class Event, class... Args>
void
basic_upstream_t::send(Args&&... args) {
    boost::optional<trace_t> restore_trace;
    if(client_trace) {
        client_trace->push(Event::alias());
        restore_trace = client_trace;
    }
    trace_t::restore_scope_t scope(restore_trace);
    session->push(encoded<Event>(channel_id, std::forward<Args>(args)...));
}

// Forwards for the upstream<T> class

template<class Tag, class Upstream> class message_queue;

} // namespace io

template<class Tag, class T>
class allowing:
    public std::enable_if<!std::is_same<typename pristine<T>::type, upstream<Tag>>::value>
{ };

template<class Tag>
class upstream {
    template<class, class> friend class io::message_queue;

    // The original untyped upstream.
    io::upstream_ptr_t ptr;

public:
    template<class Stream>
    upstream(Stream&& ptr,
             typename allowing<Tag, Stream>::type* = nullptr): ptr(std::forward<Stream>(ptr))
    { }

    template<class Event, class... Args>
    upstream<typename io::event_traits<Event>::dispatch_type>
    send(Args&&... args) {
        static_assert(
            std::is_same<typename Event::tag, Tag>::value,
            "message protocol is not compatible with this upstream"
        );

        ptr->send<Event>(std::forward<Args>(args)...);

        // Move the actual upstream pointer down the graph.
        return std::move(ptr);
    }
};

template<>
class upstream<void>
{
public:
    upstream() = default;

    template<class Stream>
    upstream(Stream&&, typename allowing<void, Stream>::type* = nullptr) {
        // Empty.
    }
};

} // namespace cocaine

#endif
