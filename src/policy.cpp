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

#include <msgpack.hpp>

#include "cocaine/policy.hpp"

using cocaine::engine::policy_t;

namespace msgpack {
    policy_t& operator >> (const object& o,
                           policy_t& policy)
    {
        if(o.type != type::ARRAY || o.via.array.size != 3) {
            throw type_error();
        }

        object &urgent = o.via.array.ptr[0],
               // &persistent = o.via.array.ptr[1],
               &timeout = o.via.array.ptr[2],
               &deadline = o.via.array.ptr[3];

        urgent >> policy.urgent;
        // persistent >> policy.persistent;
        timeout >> policy.timeout;
        deadline >> policy.deadline;

        return policy;
    }
}
