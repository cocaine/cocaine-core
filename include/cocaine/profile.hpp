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

#ifndef COCAINE_APP_PROFILE_HPP
#define COCAINE_APP_PROFILE_HPP

#include "cocaine/common.hpp"
#include "cocaine/cached.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine {

struct profile_t:
    private cached<Json::Value>
{
    profile_t(context_t& context,
              const std::string& name);

    std::string name;

    float startup_timeout;
    float heartbeat_timeout;
    float idle_timeout;
    float termination_timeout;
    unsigned long pool_limit;
    unsigned long queue_limit;
    unsigned long grow_threshold;

    struct {
        std::string type;
        Json::Value args;
    } isolate;
};

} // namespace cocaine

#endif
