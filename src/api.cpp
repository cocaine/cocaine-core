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

#include "cocaine/api/connect.hpp"
#include "cocaine/api/storage.hpp"

#include "cocaine/context.hpp"

#include "cocaine/rpc/asio/channel.hpp"

#include <asio/generic/stream_protocol.hpp>
#include <asio/local/stream_protocol.hpp>

using namespace asio;
using namespace asio::ip;

using namespace cocaine::api::details;

// Connect

basic_client_t::basic_client_t(basic_client_t&& other) {
    *this = std::move(other);
}

basic_client_t::~basic_client_t() {
    if(m_session) {
        // No error.
        m_session->detach(std::error_code());
    }
}

basic_client_t&
basic_client_t::operator=(basic_client_t&& rhs) {
    if(m_session && m_session != rhs.m_session) {
        // No error.
        m_session->detach(std::error_code());
    }

    m_session = std::move(rhs.m_session);

    return *this;
}

tcp::endpoint
basic_client_t::remote_endpoint() const {
    if(!m_session) {
        return tcp::endpoint();
    } else {
        return m_session->remote_endpoint();
    }
}

void
basic_client_t::attach(const std::shared_ptr<session<protocol_type>>& session) {
    if(m_session) {
        throw std::system_error(std::make_error_code(std::errc::already_connected));
    }

    m_session = session;
}

namespace cocaine { namespace api {

// Storage

category_traits<storage_t>::ptr_type
storage(context_t& context, const std::string& name) {
    auto it = context.config.storages.find(name);

    if(it == context.config.storages.end()) {
        throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
    }

    return context.get<storage_t>(it->second.type, context, name, it->second.args);
}

}} // namespace cocaine::api
