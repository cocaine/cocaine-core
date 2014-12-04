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

#ifndef COCAINE_IO_SOCKET_HPP
#define COCAINE_IO_SOCKET_HPP

#include "cocaine/common.hpp"

#include <system_error>

#include <fcntl.h>
#include <sys/socket.h>

namespace cocaine { namespace io {

template<class Medium>
struct socket {
    COCAINE_DECLARE_NONCOPYABLE(socket)

    typedef Medium medium_type;
    typedef typename medium_type::endpoint endpoint_type;
    typedef typename endpoint_type::protocol_type protocol_type;

    socket(const protocol_type& protocol = endpoint_type().protocol()) {
        m_fd = ::socket(protocol.family(), protocol.type(), protocol.protocol());

        if(m_fd == -1) {
            throw std::system_error(errno, std::system_category(), "unable to create a socket");
        }

        medium_type::configure(m_fd);

        ::fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(m_fd, F_SETFL, O_NONBLOCK);
    }

    explicit
    socket(endpoint_type endpoint) {
        typename endpoint_type::protocol_type protocol = endpoint.protocol();

        m_fd = ::socket(protocol.family(), protocol.type(), protocol.protocol());

        if(m_fd == -1) {
            throw std::system_error(errno, std::system_category(), "unable to create a socket");
        }

        medium_type::configure(m_fd);

        if(::connect(m_fd, endpoint.data(), endpoint.size()) != 0) {
            auto ec = std::error_code(errno, std::system_category());

            ::close(m_fd);

            throw std::system_error(
                ec,
                cocaine::format("unable to connect a socket to '%s'", endpoint)
            );
        }

        ::fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(m_fd, F_SETFL, O_NONBLOCK);
    }

    explicit
    socket(int fd):
        m_fd(fd)
    { }

   ~socket() {
        if(m_fd >= 0) ::close(m_fd);
    }

    // Moving

    socket(socket&& other):
        m_fd(-1)
    {
        *this = std::move(other);
    }

    socket&
    operator=(socket&& other) {
        std::swap(m_fd, other.m_fd);
        return *this;
    }

    // Operations

    ssize_t
    write(const char* buffer, size_t size, std::error_code& ec) {
        ssize_t length = ::write(m_fd, buffer, size);

        if(length == -1 && (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            ec = std::error_code(errno, std::system_category());
        }

        return length;
    }

    ssize_t
    read(char* buffer, size_t size, std::error_code& ec) {
        ssize_t length = ::read(m_fd, buffer, size);

        if(length == -1 && (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            ec = std::error_code(errno, std::system_category());
        }

        return length;
    }

public:
    int
    fd() const {
        return m_fd;
    }

    endpoint_type
    local_endpoint() const {
        endpoint_type endpoint;
        socklen_t size = endpoint.capacity();

        if(::getsockname(m_fd, endpoint.data(), &size) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to detect the address");
        }

        return endpoint;
    }

    endpoint_type
    remote_endpoint() const {
        endpoint_type endpoint;
        socklen_t size = endpoint.capacity();

        if(::getpeername(m_fd, endpoint.data(), &size) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to detect the address");
        }

        return endpoint;
    }

private:
    int m_fd;
};

}} // namespace cocaine::io

#endif
