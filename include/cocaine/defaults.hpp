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

#ifndef COCAINE_DEFAULTS_HPP
#define COCAINE_DEFAULTS_HPP

#include <string>

namespace cocaine {

struct defaults {
    // Default profile.
    static const bool log_output;
    static const float heartbeat_timeout;
    static const float idle_timeout;
    static const float startup_timeout;
    static const float termination_timeout;
    static const unsigned long pool_limit;
    static const unsigned long queue_limit;
    static const unsigned long concurrency;
    static const unsigned long crashlog_limit;

    // Default I/O policy.
    static const float control_timeout;

    // Default paths.
    static const std::string plugins_path;
    static const std::string runtime_path;

    // Defaults for networking.
    static const std::string endpoint;

    // Defaults for logging service.
    static const std::string log_verbosity;
    static const std::string log_timestamp;
};

} // namespace cocaine

#endif