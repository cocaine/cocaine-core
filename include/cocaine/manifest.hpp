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

#ifndef COCAINE_APP_MANIFEST_HPP
#define COCAINE_APP_MANIFEST_HPP

#include "cocaine/common.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine {

struct manifest_t {
    manifest_t(context_t& context,
               const std::string& name);

    std::string name,
                type,
                path;

    struct {
        float heartbeat_timeout;
        float suicide_timeout;
        float termination_timeout;
        unsigned int pool_limit;
        unsigned int queue_limit;
        unsigned int grow_threshold;
    } policy;

    // Path to a binary which will be used as a slave.
    std::string slave;

    // Manifest root object.
    Json::Value root;

private:
    void deploy();

private:
    context_t& m_context;
    boost::shared_ptr<logging::logger_t> m_log;
};

}

#endif
