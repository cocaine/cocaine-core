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

#include "cocaine/detail/services/node/session.hpp"
#include "cocaine/detail/services/node/messages.hpp"

#include "cocaine/traits/literal.hpp"

using namespace cocaine::engine;
using namespace cocaine::io;

session_t::session_t(uint64_t id_, const api::event_t& event_, const api::stream_ptr_t& upstream_):
    id(id_),
    event(event_),
    upstream(upstream_),
    m_state(state::open)
{
    m_encoder.reset(new encoder<writable_stream<io::socket<local>>>());

    // Cache the invocation command right away.
    send<rpc::invoke>(event.name);
}

void
session_t::attach(const std::shared_ptr<writable_stream<io::socket<local>>>& downstream) {
    // Flush all the cached messages into the downstream.
    m_encoder->attach(downstream);
}

void
session_t::detach() {
    close();

    // Disable the session.
    m_encoder.reset();
}

void
session_t::close() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_state == state::open) {
        m_encoder->write<rpc::choke>(id);

        // There shouldn't be any other chunks after that.
        m_state = state::closed;
    }
}

session_t::downstream_t::downstream_t(const std::shared_ptr<session_t>& parent_):
    parent(parent_)
{ }

session_t::downstream_t::~downstream_t() {
    close();
}

void
session_t::downstream_t::write(const char* chunk, size_t size) {
    parent->send<rpc::chunk>(literal_t { chunk, size });
}

void
session_t::downstream_t::error(int code, const std::string& reason) {
    parent->send<rpc::error>(code, reason);
}

void
session_t::downstream_t::close() {
    parent->close();
}
