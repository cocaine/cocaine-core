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

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/pipe.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

using namespace cocaine::io;

acceptor_t::acceptor_t(const std::string& path,
                       int backlog):
    m_fd(::socket(AF_LOCAL, SOCK_STREAM, 0))
{
    if(m_fd == -1) {
        throw io_error_t("unable to create an acceptor");
    }

    // Set non-blocking and close-on-exec options.
    configure(m_fd);

#ifdef _GNU_SOURCE
    struct sockaddr_un address = { AF_LOCAL, { 0 } };
#else
    struct sockaddr_un address = { sizeof(sockaddr_un), AF_LOCAL, { 0 } };
#endif

    ::memcpy(address.sun_path, path.c_str(), path.size());

    if(::bind(m_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1) {
        throw io_error_t("unable to bind an acceptor on '%s'", path);
    }

    ::listen(m_fd, backlog);
}

acceptor_t::~acceptor_t() {
    if(m_fd == -1) {
        return;
    }

#ifdef _GNU_SOURCE
    struct sockaddr_un address = { AF_LOCAL, { 0 } };
#else
    struct sockaddr_un address = { sizeof(sockaddr_un), AF_LOCAL, { 0 } };
#endif
    
    socklen_t length = sizeof(address);

    ::getsockname(m_fd, reinterpret_cast<sockaddr*>(&address), &length);

    if(::close(m_fd) != 0) {
        // Log.
    }

    if(::unlink(address.sun_path) != 0) {
        // Log.
    }
}

acceptor_t::acceptor_t(acceptor_t&& other):
    m_fd(-1)
{
    *this = std::move(other);
}

acceptor_t&
acceptor_t::operator=(acceptor_t&& other) {
    std::swap(m_fd, other.m_fd);
    return *this;
}

std::shared_ptr<pipe_t>
acceptor_t::accept() {
#ifdef _GNU_SOURCE
    struct sockaddr_un address = { AF_LOCAL, { 0 } };
#else
    struct sockaddr_un address = { sizeof(sockaddr_un), AF_LOCAL, { 0 } };
#endif
    
    socklen_t length = sizeof(address);

    int fd = ::accept(m_fd, reinterpret_cast<sockaddr*>(&address), &length);

    if(fd == -1) {
        switch(errno) {
            case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
            case EWOULDBLOCK:
#endif
            case EINTR:
                return std::shared_ptr<pipe_t>();

            default:
                throw io_error_t("unable to accept a connection");
        }
    }

    // Set non-blocking and close-on-exec options.
    configure(fd);

    return std::make_shared<pipe_t>(fd);
}

void
acceptor_t::configure(int fd) {
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
    ::fcntl(fd, F_SETFL, O_NONBLOCK);
}

pipe_link_t
cocaine::io::link() {
    int fds[2] = { -1, -1 };

    if(::socketpair(AF_LOCAL, SOCK_STREAM, 0, fds) != 0) {
        throw io_error_t("unable to create a link");
    }

    return std::make_pair(
        std::make_shared<pipe_t>(fds[0]),
        std::make_shared<pipe_t>(fds[1])
    );
}
