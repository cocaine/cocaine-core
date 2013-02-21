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

#ifndef COCAINE_ASIO_TCP_HPP
#define COCAINE_ASIO_TCP_HPP

#include "cocaine/common.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

namespace cocaine { namespace io {

struct tcp {
    // typedef acceptor<tcp> acceptor;
    // typedef pipe<tcp> pipe;

    struct endpoint {
        typedef sockaddr    base_type;
        typedef sockaddr_in address_type;
        typedef socklen_t   size_type;

        endpoint() {
            std::memset(&m_data, 0, sizeof(m_data));
        }

        endpoint(const std::string& address,
                 uint16_t port)
        {
            std::memset(&m_data, 0, sizeof(m_data));

            m_data.tcp4.sin_family = tcp::family();
            m_data.tcp4.sin_port = htons(port);

            if(::inet_pton(tcp::family(), address.c_str(), &m_data.tcp4.sin_addr) == 0) {
                throw cocaine::io_error_t("endpoint address '%s' is invalid", address);
            }
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

        friend
        std::ostream&
        operator<<(std::ostream& stream,
                   const tcp::endpoint& endpoint)
        {
            char result[INET_ADDRSTRLEN];

            ::inet_ntop(
                tcp::family(),
                &endpoint.m_data.tcp4.sin_addr,
                result,
                INET_ADDRSTRLEN
            );

            return stream << cocaine::format(
                "%s:%d",
                result,
                ntohs(endpoint.m_data.tcp4.sin_port)
            );
        }

    private:
        union {
            base_type    base;
            address_type tcp4;
        } m_data;
    };

    typedef endpoint endpoint_type;

    static
    int
    family() {
        return AF_INET;
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
