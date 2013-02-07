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
#include "cocaine/birth_control.hpp"
#include "cocaine/messaging.hpp"
#include "cocaine/slave.hpp"

#include "cocaine/api/event.hpp"

#include <sstream>

namespace cocaine { namespace engine {

struct session_t:
    public birth_control<session_t>
{
    session_t(uint64_t id,
              const api::event_t& event,
              const boost::shared_ptr<api::stream_t>& upstream);

    void
    attach(slave_t * const slave);

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
    const boost::shared_ptr<api::stream_t> upstream;

private:
    // Responsible slave.
    slave_t * m_slave;

    typedef std::vector<
        std::string
    > message_cache_t;

    // Message cache.
    message_cache_t m_cache;
    boost::mutex m_mutex;
};

template<class Event, typename... Args>
void
session_t::send(Args&&... args) {
    boost::unique_lock<boost::mutex> lock(m_mutex);

    // Pre-pack the message.
    auto blob = io::codec::pack<Event>(
        id,
        std::forward<Args>(args)...
    );

    if(m_slave) {
        m_slave->send(blob);
    } else {
        m_cache.emplace_back(std::move(blob));
    }
}

}}

#endif
