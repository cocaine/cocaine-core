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

#ifndef COCAINE_ASIO_LOCAL_HPP
#define COCAINE_ASIO_LOCAL_HPP

#include "cocaine/common.hpp"

#include <sys/socket.h>
#include <sys/un.h>

// According to unix(7), this is the maximum length of a local socket path.
#define UNIX_PATH_MAX 108

namespace cocaine { namespace io {

struct local {
    // typedef acceptor<local> acceptor;
    // typedef pipe<local> pipe;

    struct endpoint {
        typedef sockaddr    base_type;
        typedef sockaddr_un address_type;
        typedef socklen_t   size_type;

        endpoint() {
            std::memset(&m_data, 0, sizeof(m_data));
        }

        endpoint(const std::string& address) {
            std::memset(&m_data, 0, sizeof(m_data));

            m_data.local.sun_family = local::family();

            if(address.size() >= UNIX_PATH_MAX) {
                throw cocaine::io_error_t("stream address '%s' exceeds the maximum allowed length");
            }

            std::memcpy(m_data.local.sun_path, address.c_str(), address.size());
        }

        base_type*
        data() {
            return &m_data.base;
        }

        const base_type*
        data() const {
            return &m_data.base;
        }

        size_type
        size() const {
            return sizeof(m_data);
        }

        std::string
        string() const {
            return m_data.local.sun_path;
        }

        friend
        std::ostream&
        operator<<(std::ostream& stream,
                   const local::endpoint& endpoint)
        {
            return stream << endpoint.string();
        }

    private:
        union {
            base_type    base;
            address_type local;
        } m_data;
    };

    static
    int
    family() {
        return AF_LOCAL;
    }

    static
    int
    type() {
        return SOCK_STREAM;
    }

    static
    int
    protocol() {
        return 0;
    }
};

}} // namespace cocaine::io

#endif
