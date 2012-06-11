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

#include "native_job.hpp"

#include "cocaine/rpc.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::drivers;
using namespace cocaine::io;

native_job_t::native_job_t(const std::string& event, 
                           const blob_t& request,
                           const policy_t& policy,
                           channel_t& channel,
                           const route_t& route,
                           const std::string& tag):
    job_t(event, request, policy),
    m_channel(channel),
    m_route(route),
    m_tag(tag)
{
    rpc::packed<dealer::acknowledgement> pack;
    send(pack);
}

void native_job_t::react(const events::chunk& event) {
    rpc::packed<dealer::chunk> pack(event.message);
    send(pack);
}

void native_job_t::react(const events::error& event) {
    rpc::packed<dealer::error> pack(event.code, event.message);
    send(pack);
}

void native_job_t::react(const events::choke& event) {
    rpc::packed<dealer::choke> pack;
    send(pack);
}
