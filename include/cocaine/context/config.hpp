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

#include <boost/optional/optional_fwd.hpp>

#include <map>
#include <string>
#include <vector>

namespace cocaine {

// Configuration

struct config_t {
public:
    virtual
    ~config_t() {}

    struct path_t {
        virtual
        ~path_t() {}

        // Paths to search cocaine plugins for
        virtual
        const std::vector<std::string>&
        plugins() const = 0;

        // Path used to create cocaine stuff like unix socket for workers, cocaine pid file, etc
        virtual
        const std::string&
        runtime() const = 0;
    };

    struct network_t {
        struct ports_t {
            virtual
            ~ports_t() {}

            // Pinned services - services bound to some specific port
            // Returns map of service name -> port number
            virtual
            const std::map<std::string, port_t>&
            pinned() const = 0;

             // Diapason of ports to bound on for all non-pinned services
            virtual
            const std::tuple<port_t, port_t>&
            shared() const = 0;
        };

        virtual
        ~network_t() {}

        virtual
        const ports_t&
        ports() const = 0;

        // An endpoint where all the services will be bound. Note that binding on [::] will bind on
        // 0.0.0.0 too as long as the "net.ipv6.bindv6only" sysctl is set to 0 (default).
        virtual
        const std::string&
        endpoint() const = 0;

        // Local hostname. In case it can't be automatically detected by resolving a CNAME for the
        // contents of /etc/hostname via the default system resolver, it can be configured manually.
        virtual
        const std::string&
        hostname() const = 0;

        // I/O thread pool size.
        virtual
        size_t
        pool() const = 0;
    };

    struct logging_t {
        virtual
        ~logging_t() {}

        virtual
        const dynamic_t&
        loggers() const = 0;

        virtual
        logging::priorities
        severity() const = 0;
    };

    struct component_t {
        virtual
        ~component_t() {}

        virtual
        const std::string&
        type() const = 0;

        virtual
        const dynamic_t&
        args() const = 0;
    };

    // Component group such as storages, services or unicorns
    struct component_group_t {
        typedef std::function<void(const std::string& name, const component_t&)> callable_t;

        virtual
        ~component_group_t() {}

        virtual
        size_t size() const = 0;

        virtual
        boost::optional<const component_t&>
        get(const std::string& name) const = 0;

        // Apply callable to each component stored in component group
        virtual
        void
        each(const callable_t& callable) const = 0;
    };

    virtual
    const network_t&
    network() const = 0;

    virtual
    const logging_t&
    logging() const = 0;

    virtual
    const path_t&
    path() const = 0;

    virtual
    const component_group_t&
    services() const = 0;

    virtual
    const component_group_t&
    storages() const = 0;

    virtual
    const component_group_t&
    unicorns() const = 0;

    virtual
    const component_group_t&
    component_group(const std::string& name) const = 0;

    virtual
    const std::string&
    uuid() const = 0;

    static
    int
    versions();

};

std::unique_ptr<config_t>
make_config(const std::string& source);

} // namespace cocaine

#endif
