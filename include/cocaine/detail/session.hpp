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

#ifndef COCAINE_SESSION_HPP
#define COCAINE_SESSION_HPP

#include "cocaine/common.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/asio/local.hpp"
#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/writable_stream.hpp"

#include "cocaine/detail/atomic.hpp"

#include "cocaine/rpc/encoder.hpp"

namespace cocaine { namespace engine {

struct session_t {
    COCAINE_DECLARE_NONCOPYABLE(session_t)

    session_t(uint64_t id, const api::event_t& event, const api::stream_ptr_t& upstream);

    struct downstream_t:
        public api::stream_t
    {
        downstream_t(const std::shared_ptr<session_t>& parent);

        virtual
       ~downstream_t();

        virtual
        void
        write(const char* chunk, size_t size);

        virtual
        void
        error(int code, const std::string& reason);

        virtual
        void
        close();

    private:
        std::shared_ptr<session_t> parent;
    };

    void
    attach(const std::shared_ptr<io::writable_stream<io::socket<io::local>>>& downstream);

    void
    detach();

    void
    close();

public:
    // Session ID.
    const uint64_t id;

    // Session event type and execution policy.
    const api::event_t event;

    // Client's upstream for response delivery.
    const std::shared_ptr<api::stream_t> upstream;

private:
    template<class Event, typename... Args>
    void
    send(Args&&... args);

private:
    std::unique_ptr<
        io::encoder<io::writable_stream<io::socket<io::local>>>
    > m_encoder;

    struct state {
        enum value: int { open, closed };
    };

    // Session state.
    std::atomic<int> m_state;
};

template<class Event, typename... Args>
void
session_t::send(Args&&... args) {
    if(m_state == state::open) {
        m_encoder->write<Event>(id, std::forward<Args>(args)...);
    } else {
        throw cocaine::error_t("the session is no longer valid");
    }
}

}}

#endif
