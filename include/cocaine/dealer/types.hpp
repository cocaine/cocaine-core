/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
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

#ifndef _COCAINE_DEALER_TYPES_HPP_INCLUDED_
#define _COCAINE_DEALER_TYPES_HPP_INCLUDED_

#include <boost/flyweight.hpp>
#include <msgpack.hpp>

namespace cocaine {
namespace dealer {

struct policy_t {
    policy_t():
        urgent(false),
        timeout(0.0f),
        deadline(0.0f)
    { }

    policy_t(bool urgent_, double timeout_, double deadline_):
        urgent(urgent_),
        timeout(timeout_),
        deadline(deadline_)
    { }

    bool urgent;
    double timeout;
    double deadline;

    MSGPACK_DEFINE(urgent, timeout, deadline);
};

enum error_code {
    request_error   = 400,
    location_error  = 404,
    server_error    = 500,
    app_error       = 502,
    resource_error  = 503,
    timeout_error   = 504,
    deadline_error  = 520
};

enum rpc_code {
    acknowledgement = 1,
    chunk           = 4,
    error           = 5,
    choke           = 6
};

}}

#endif // _COCAINE_DEALER_TYPES_HPP_INCLUDED_
