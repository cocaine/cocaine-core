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

#ifndef COCAINE_IO_ACCEPTOR_HPP
#define COCAINE_IO_ACCEPTOR_HPP

#include "cocaine/asio/socket.hpp"

namespace cocaine { namespace io {

template<class Medium>
struct acceptor {
    COCAINE_DECLARE_NONCOPYABLE(acceptor)

    typedef Medium medium_type;
    typedef typename medium_type::endpoint endpoint_type;
    typedef typename endpoint_type::size_type size_type;

    // Type of the socket this acceptor yields on a new connection.
    typedef socket<medium_type> socket_type;

    acceptor(endpoint_type endpoint, int backlog = 1024) {
        medium_type medium;

        m_fd = ::socket(medium.family(), medium.type(), medium.protocol());

        if(m_fd == -1) {
            throw io_error_t("unable to create an acceptor");
        }

        ::fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(m_fd, F_SETFL, O_NONBLOCK);

        const int enable = 1;

        ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

        if(::bind(m_fd, endpoint.data(), endpoint.size()) != 0) {
            throw io_error_t("unable to bind an acceptor on '%s'", endpoint);
        }

        if(::listen(m_fd, backlog) != 0) {
            throw io_error_t("unable to activate an acceptor on '%s'", endpoint);
        }
    }

   ~acceptor() {
        if(m_fd >= 0 && ::close(m_fd) != 0) {
            // Log.
        }
    }

    // Moving

    acceptor(acceptor&& other):
        m_fd(-1)
    {
        *this = std::move(other);
    }

    acceptor&
    operator=(acceptor&& other) {
        std::swap(m_fd, other.m_fd);
        return *this;
    }

    // Operations

    std::shared_ptr<socket_type>
    accept() {
        endpoint_type endpoint;
        size_type size = endpoint.size();

        int fd = ::accept(m_fd, endpoint.data(), &size);

        if(fd == -1) {
            switch(errno) {
                case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
                case EWOULDBLOCK:
#endif
                case EINTR:
                    return std::shared_ptr<socket_type>();

                default:
                    throw io_error_t("unable to accept a connection");
            }
        }

        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(fd, F_SETFL, O_NONBLOCK);

        return std::make_shared<socket_type>(fd);
    }

public:
    int
    fd() const {
        return m_fd;
    }

    endpoint_type
    local_endpoint() const {
        endpoint_type endpoint;
        size_type size = endpoint.size();

        if(::getsockname(m_fd, endpoint.data(), &size) != 0) {
            throw io_error_t("unable to determine the local socket address");
        }

        return endpoint;
    }

private:
    int m_fd;
};

template<class EndpointType>
std::pair<
    std::shared_ptr<socket<EndpointType>>,
    std::shared_ptr<socket<EndpointType>>
>
link() {
    int fd[] = { -1, -1 };

    if(::socketpair(AF_LOCAL, SOCK_STREAM, 0, fd)) {
        throw io_error_t("unable to create linked sockets");
    }

    return std::make_pair(
        std::make_shared<socket<EndpointType>>(fd[0]),
        std::make_shared<socket<EndpointType>>(fd[1])
    );
}

}} // namespace cocaine::io

#endif
