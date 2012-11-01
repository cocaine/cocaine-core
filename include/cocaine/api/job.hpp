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

#ifndef COCAINE_JOB_HPP
#define COCAINE_JOB_HPP

#include "cocaine/common.hpp"
#include "cocaine/events.hpp"
#include "cocaine/policy.hpp"

#include "cocaine/helpers/birth_control.hpp"

namespace cocaine { namespace engine {

struct job_t:
    public birth_control<job_t>
{
    job_t(const std::string& event_):
        event(event_)
    { }

    job_t(const std::string& event_,
          policy_t policy_):
        event(event_),
        policy(policy_)
    { }
    
    virtual
    ~job_t() {
        // Empty.
    }

public:
    virtual
    void
    react(const events::chunk&) { };
    
    virtual
    void
    react(const events::error&) { };
    
    virtual
    void
    react(const events::choke&) { };

public:
    // Wrapped event type.
    const std::string event;
    
    // Execution policy.
    const policy_t policy;
};

}} // namespace cocaine::engine

#endif
