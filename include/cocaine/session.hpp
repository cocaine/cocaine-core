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

#include "cocaine/helpers/birth_control.hpp"

namespace cocaine { namespace engine {

struct session_t:
    public birth_control<session_t>
{
    session_t(const unique_id_t& id,
              const api::event_t& event,
              const boost::shared_ptr<api::stream_t>& upstream,
              const boost::shared_ptr<api::stream_t>& downstream);

    bool
    closed() const;

    // Session ID.
    const unique_id_t& id;

    // Session event type.
    const api::event_t& event;

    // Session streams

    typedef boost::shared_ptr<
        api::stream_t
    > stream_ptr_t;

    const stream_ptr_t upstream,
                       downstream;
};

}}

#endif
