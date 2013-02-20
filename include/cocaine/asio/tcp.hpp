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

namespace cocaine { namespace io { namespace tcp {

struct endpoint {
    typedef sockaddr    base_type;
    typedef sockaddr_in address_type;

    endpoint(const std::string& address,
             uint16_t port)
    {
        std::memset(&m_data, 0, sizeof(m_data));

        m_data.tcp4.sin_family = AF_INET;
        m_data.tcp4.sin_port = htons(port);

        if(::inet_pton(AF_INET, address.c_str(), &m_data.tcp4.sin_addr) == 0) {
            throw cocaine::io_error_t("endpoint address '%s' is invalid", address);
        }
    }

    int
    create() const {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        int enable = 1;

        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

        return fd;
    }

    const base_type*
    data() const {
        return &m_data.base;
    }

    socklen_t
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
            AF_INET,
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

}}} // namespace cocaine::io::tcp

#endif
