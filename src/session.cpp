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

using namespace cocaine::engine;
using namespace cocaine::io;

session_t::session_t(uint64_t id_,
                     const api::event_t& event_,
                     const std::shared_ptr<api::stream_t>& upstream_):
    id(id_),
    event(event_),
    upstream(upstream_)
{
    m_encoder.reset(new encoder<writable_stream<io::socket<local>>>());
}

void
session_t::attach(const std::shared_ptr<writable_stream<io::socket<local>>>& stream) {
    std::unique_lock<std::mutex> lock(m_mutex);

    // Flush all the cached messages into the stream.
    m_encoder->attach(stream);
}

void
session_t::detach() {
    std::unique_lock<std::mutex> lock(m_mutex);

    // Disable the session.
    m_encoder.reset();
}
