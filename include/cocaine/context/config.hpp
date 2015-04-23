/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_CONTEXT_CONFIG_HPP
#define COCAINE_CONTEXT_CONFIG_HPP

#include "cocaine/common.hpp"
#include "cocaine/dynamic/dynamic.hpp"

#include <asio/ip/address.hpp>

#include <blackhole/blackhole.hpp>

namespace cocaine {

// Configuration

struct config_t {
    config_t(const std::string& source);

    static
    int
    versions();

public:
    struct {
        std::string plugins;
        std::string runtime;
    } path;

    struct {
        // An endpoint where all the services will be bound. Note that binding on [::] will bind on
        // 0.0.0.0 too as long as the "net.ipv6.bindv6only" sysctl is set to 0 (default).
        asio::ip::address endpoint;

        // Local hostname. In case it can't be automatically detected by resolving a CNAME for the
        // contents of /etc/hostname via the default system resolver, it can be configured manually.
        std::string hostname;

        // I/O thread pool size.
        size_t pool;

        struct {
            // Pinned ports for static service port allocation.
            std::map<std::string, port_t> pinned;

            // Port range to populate the dynamic port pool for service port allocation.
            std::tuple<port_t, port_t> shared;
        } ports;
    } network;

    struct logging_t {
        struct logger_t {
            logging::priorities verbosity;
            std::string timestamp;
            blackhole::log_config_t config;
        };

        std::map<std::string, logger_t> loggers;
    } logging;

    struct component_t {
        std::string type;
        dynamic_t   args;
    };

    typedef std::map<std::string, component_t> component_map_t;

    component_map_t services;
    component_map_t storages;

#ifdef COCAINE_ALLOW_RAFT
    bool create_raft_cluster;
#endif
};

} // namespace cocaine

#endif
