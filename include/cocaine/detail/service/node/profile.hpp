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

#ifndef COCAINE_ENGINE_PROFILE_HPP
#define COCAINE_ENGINE_PROFILE_HPP

#include "cocaine/common.hpp"
#include "cocaine/detail/cached.hpp"

#include "cocaine/dynamic.hpp"

namespace cocaine {

struct profile_t:
    cached<dynamic_t>
{
    profile_t(context_t& context, const std::string& name);

    // The profile name.
    std::string name;

    // Copy all the slave output to the runtime log.
    bool log_output;

    // Timeouts.
    float heartbeat_timeout;
    float idle_timeout;
    float startup_timeout;
    float termination_timeout;

    // Limits.
    unsigned long concurrency;
    unsigned long crashlog_limit;
    unsigned long grow_threshold;
    unsigned long pool_limit;
    unsigned long queue_limit;

    // NOTE: The slave processes are launched in sandboxed environments,
    // called isolates. This one describes the isolate type and arguments.
    config_t::component_t isolate;
};

} // namespace cocaine

#endif
