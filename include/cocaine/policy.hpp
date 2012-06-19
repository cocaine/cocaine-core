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

#ifndef COCAINE_JOB_POLICY_HPP
#define COCAINE_JOB_POLICY_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace engine {

// Job policy
// ----------

struct policy_t {
    policy_t():
        urgent(false),
        persistent(false),
        timeout(0.0f),
        deadline(0.0f)
    { }

    policy_t(bool urgent_, double timeout_, double deadline_):
        urgent(urgent_),
        persistent(false),
        timeout(timeout_),
        deadline(deadline_)
    { }

    bool urgent;
    bool persistent;
    double timeout;
    double deadline;
};

}}

namespace msgpack {
    using cocaine::engine::policy_t;

    policy_t& operator >> (const object& o,
                           policy_t& policy);

    template<class Stream>
    inline packer<Stream>& operator << (packer<Stream>& packer,
                                        const policy_t& policy)
    {
        packer.pack_array(3);
        packer.pack(policy.urgent);
        // packer.pack(policy.persistent);
        packer.pack(policy.timeout);
        packer.pack(policy.deadline);
    }
}

#endif
