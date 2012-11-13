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

#include "cocaine/session.hpp"

#include "cocaine/engine.hpp"
#include "cocaine/rpc.hpp"
#include "cocaine/slave.hpp"

#include "cocaine/api/event.hpp"

#include "cocaine/traits/unique_id.hpp"

using namespace cocaine::engine;

session_t::session_t(const boost::shared_ptr<event_t>& event,
                     engine_t * const engine):
    state(states::inactive),
    ptr(event),
    m_engine(engine),
    m_slave(NULL)
{ }

session_t::~session_t() {
    if(state != states::closed) {
        ptr->close();
    }
}

void
session_t::attach(slave_t * const slave) {
    // TEST: Sessions can only be attached when inactive.
    BOOST_ASSERT(state == states::inactive && !m_slave);
    
    state = states::active;
    m_slave = slave;
    
    boost::unique_lock<boost::mutex> lock(m_mutex);

    if(!m_cache.empty()) {
        for(chunk_list_t::const_iterator it = m_cache.begin();
            it != m_cache.end();
            ++it)
        {
            push(it->data(), it->size());
        }
        
        m_cache.clear();
    }
}

void
session_t::push(const void * chunk,
                size_t size)
{
    switch(state) {
        case states::active: {
            // TEST: An active session should always have a controlling slave.
            BOOST_ASSERT(m_slave);

            zmq::message_t message(size);

            memcpy(
                message.data(),
                chunk,
                size
            );

            m_engine->send(
                m_slave->id(),
                io::message<rpc::chunk>(id, message)
            );
        
            break;
        }

        case states::inactive: {
            boost::unique_lock<boost::mutex> lock(m_mutex);

            // NOTE: Put the new chunk into the cache because the session is
            // not yet assigned to a slave.
            m_cache.emplace_back(static_cast<const char*>(chunk), size);

            break;
        }

        case states::closed:
            throw cocaine::error_t("the session has been closed");
    }
}

void
session_t::close() {
    if(state == states::active) {
        // TEST: An active session should always have a controlling slave.
        BOOST_ASSERT(m_slave);

        m_engine->send(
            m_slave->id(),
            io::message<rpc::choke>(id)
        );
    }

    ptr->close();

    state = states::closed;
}
