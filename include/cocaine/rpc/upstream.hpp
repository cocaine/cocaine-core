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
#include "cocaine/trace/trace.hpp"

namespace cocaine {

template<class Tag> class upstream;

namespace io {

class basic_upstream_t {
    const std::shared_ptr<session_t> m_session;
    const uint64_t m_channel_id;

public:
    basic_upstream_t(const std::shared_ptr<session_t>& session, uint64_t channel_id):
        m_session(session),
        m_channel_id(channel_id)
    { }

    uint64_t
    channel_id() const {
        return m_channel_id;
    }

    auto
    session() const -> std::shared_ptr<session_t> {
        return m_session;
    }

    /// Detaches underlying session and closes connection.
    ///
    /// This will discard all active chanels and close connecton to client,
    /// it can only be used as a last resort to signal unrecoverable error.
    __attribute__((deprecated("use `session()` instead")))
    void
    detach_session(const std::error_code& ec) {
        m_session->detach(ec);
    }

    void
    send(encoder_t::message_type message) {
        m_session->push(std::move(message));
    };

    template<class Event, class... Args>
    void
    send(Args&&... args) {
        send<Event>({}, std::forward<Args>(args)...);
    }

    template<class Event, class... Args>
    void
    send(hpack::header_storage_t headers, Args&&... args) {
        send(encoded<Event>(m_channel_id, std::move(headers), std::forward<Args>(args)...));
    }
};

// Forwards for the upstream<T> class

template<class Tag> class message_queue;

} // namespace io

template<class Tag, class T>
class allowing:
    public std::enable_if<!std::is_same<typename pristine<T>::type, upstream<Tag>>::value>
{ };

template<class Tag>
class upstream {
    template<class> friend class io::message_queue;

    // The original untyped upstream.
    io::upstream_ptr_t ptr;

public:
    template<class Stream>
    upstream(Stream&& ptr,
             typename allowing<Tag, Stream>::type* = nullptr): ptr(std::forward<Stream>(ptr))
    { }

    auto
    session() const -> std::shared_ptr<session_t> {
        return ptr->session();
    }

    template<class Event, class... Args>
    upstream<typename io::event_traits<Event>::dispatch_type>
    send(hpack::header_storage_t headers, Args&&... args) {
        static_assert(
            std::is_same<typename Event::tag, Tag>::value,
            "message protocol is not compatible with this upstream"
        );

        ptr->send<Event>(std::move(headers), std::forward<Args>(args)...);

        // Move the actual upstream pointer down the graph.
        return std::move(ptr);
    }

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
