/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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
#include "cocaine/asio/pipe.hpp"
#include "cocaine/birth_control.hpp"
#include "cocaine/rpc.hpp"

#include <mutex>

namespace cocaine { namespace engine {

struct session_t:
    public birth_control<session_t>,
    boost::noncopyable
{
    session_t(uint64_t id,
              const api::event_t& event,
              const std::shared_ptr<api::stream_t>& upstream);

    void
    attach(const std::shared_ptr<io::writable_stream<io::pipe_t>>& stream);

    void
    detach();

    template<class Event, typename... Args>
    void
    send(Args&&... args);

public:
    // Session ID.
    const uint64_t id;

    // Session event type and execution policy.
    const api::event_t event;

    // Client's upstream for result delivery.
    const std::shared_ptr<api::stream_t> upstream;

private:
    std::unique_ptr<
        io::encoder<io::pipe_t>
    > m_encoder;

    // Session interlocking.
    std::mutex m_mutex;
};

template<class Event, typename... Args>
void
session_t::send(Args&&... args) {
    std::unique_lock<std::mutex> lock(m_mutex);

    if(m_encoder) {
        m_encoder->write<Event>(id, std::forward<Args>(args)...);
    } else {
        throw cocaine::error_t("stream is no longer valid");
    }
}

}}

#endif
