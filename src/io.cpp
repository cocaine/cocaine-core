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

#include "cocaine/io.hpp"

#include "cocaine/context.hpp"

using namespace cocaine::io;

socket_t::socket_t(context_t& context, int type):
    m_context(context),
    m_socket(context.io(), type)
{
    int linger = 0;
   
    // Disable lingering on context termination. 
    setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
} 

void
socket_t::bind(const std::string& endpoint) {
    m_socket.bind(endpoint.c_str());

    // Try to determine the connection string for clients.
    // FIXME: Fix it when migrating to ZeroMQ 3.1+
    size_t position = endpoint.find_last_of(":");

    if(position != std::string::npos) {
        m_endpoint = std::string("tcp://")
                     + m_context.config.runtime.hostname
                     + endpoint.substr(position, std::string::npos);
    } else {
        m_endpoint = "<local>";
    }
}

void
socket_t::connect(const std::string& endpoint) {
    m_socket.connect(endpoint.c_str());
}

void
socket_t::drop() {
    zmq::message_t null;

    while(more()) {
        recv(&null);
    }
}
