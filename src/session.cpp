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

#include "cocaine/rpc.hpp"

using namespace cocaine::engine;

session_t::session_t(uint64_t id_,
                     const api::event_t& event_,
                     const boost::shared_ptr<api::stream_t>& upstream_):
    id(id_),
    event(event_),
    upstream(upstream_),
    m_slave(NULL)
{
    // NOTE: This will go to cache, but we save on this serialization later.
    send<io::rpc::invoke>(event.type);
}

void
session_t::attach(slave_t * const slave) {
    BOOST_ASSERT(!m_slave);

    boost::unique_lock<boost::mutex> lock(m_mutex);

    m_slave = slave;

    for(message_cache_t::iterator it = m_cache.begin();
        it != m_cache.end();
        ++it)
    {
        m_slave->send(*it);
    }
}

void
session_t::detach() {
    BOOST_ASSERT(m_slave);

    boost::unique_lock<boost::mutex> lock(m_mutex);

    // NOTE: In case the client managed to get the shared_ptr to the
    // session the same moment when it got erased in the slave's session map.
    m_slave = NULL;
}
