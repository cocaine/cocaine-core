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

#include "cocaine/api/stream.hpp"

using namespace cocaine::engine;

session_t::session_t(const api::event_t& event_,
                     const boost::shared_ptr<api::stream_t>& upstream_):
    event(event_),
    upstream(upstream_),
    m_slave(NULL)
{ }

void
session_t::abandon(error_code code,
                   const std::string& message)
{
    upstream->error(code, message);
}
