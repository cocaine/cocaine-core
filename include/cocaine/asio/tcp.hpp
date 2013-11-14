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

#include <boost/asio/ip/tcp.hpp>

namespace cocaine { namespace io {

struct tcp {
    typedef boost::asio::ip::tcp::endpoint endpoint;
    typedef boost::asio::ip::tcp::resolver resolver;

    static
    void
    configure(int fd) {
        int enable = 1;

        // Enable TCP_NODELAY option to boost the performance a little.
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

        // Enable keepalive probes.
        ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
    }
};

}} // namespace cocaine::io

#endif
