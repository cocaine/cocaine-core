/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_IO_TCP_HPP
#define COCAINE_IO_TCP_HPP

#include "cocaine/common.hpp"

#include <cstring>
#include <system_error>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

namespace cocaine { namespace io {

struct tcp {
    static
    void
    configure(int fd) {
        int enable = 1;

        // Enable TCP_NODELAY option to boost the performance a little.
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
    }

    struct endpoint {
        typedef sockaddr    base_type;
        typedef sockaddr_in address_type;
        typedef socklen_t   size_type;

        endpoint() {
            std::memset(&m_data, 0, sizeof(m_data));
        }

        endpoint(const std::string& address, uint16_t port) {
            std::memset(&m_data, 0, sizeof(m_data));

            m_data.tcp4.sin_family = tcp::family();
            m_data.tcp4.sin_port = htons(port);

            if(::inet_pton(tcp::family(), address.c_str(), &m_data.tcp4.sin_addr) == 0) {
                throw std::system_error(
                    errno,
                    std::system_category(),
                    cocaine::format("unable to parse '%s' as an endpoint address", address)
                );
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

        std::string
        address() const {
            char result[INET_ADDRSTRLEN];

            if(::inet_ntop(tcp::family(), &m_data.tcp4.sin_addr, result, INET_ADDRSTRLEN) == nullptr) {
                throw std::system_error(
                    errno,
                    std::system_category(),
                    "unable to format the endpoint address"
                );
            }

            return result;
        }

        uint16_t
        port() const {
            return ntohs(m_data.tcp4.sin_port);
        }

        void
        port(uint16_t port_) {
            m_data.tcp4.sin_port = htons(port_);
        }

        std::string
        string() const {
            return cocaine::format("%s:%d", address(), port());
        }

        friend
        std::ostream&
        operator<<(std::ostream& stream, const tcp::endpoint& endpoint) {
            return stream << endpoint.string();
        }

    private:
        union {
            base_type    base;
            address_type tcp4;
        } m_data;
    };

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
        return IPPROTO_TCP;
    }
};

}} // namespace cocaine::io

#endif
