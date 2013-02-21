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

#ifndef COCAINE_ASIO_ACCEPTOR_HPP
#define COCAINE_ASIO_ACCEPTOR_HPP

#include "cocaine/asio/pipe.hpp"

namespace cocaine { namespace io {

template<class MediumType>
struct acceptor:
    boost::noncopyable
{
    typedef MediumType                          medium_type;
    typedef pipe<MediumType>                    pipe_type;
    typedef typename medium_type::endpoint_type endpoint_type;
    typedef typename endpoint_type::size_type   size_type;

    acceptor(endpoint_type endpoint,
             int backlog = 1024)
    {
        m_fd = ::socket(
            medium_type::family(),
            medium_type::type(),
            medium_type::protocol()
        );

        if(m_fd == -1) {
            throw io_error_t("unable to create an acceptor");
        }

        ::fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(m_fd, F_SETFL, O_NONBLOCK);

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

    std::shared_ptr<pipe_type>
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
                    return std::shared_ptr<pipe_type>();

                default:
                    throw io_error_t("unable to accept a connection");
            }
        }

        ::fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(m_fd, F_SETFL, O_NONBLOCK);

        return std::make_shared<pipe_type>(fd);
    }

public:
    int
    fd() const {
        return m_fd;
    }

private:
    int m_fd;
};

template<class EndpointType>
std::pair<
    std::shared_ptr<pipe<EndpointType>>,
    std::shared_ptr<pipe<EndpointType>>
>
link() {
    int fd[] = { -1, -1 };

    if(::socketpair(AF_LOCAL, SOCK_STREAM, 0, fd)) {
        throw io_error_t("unable to create linked streams");
    }

    return std::make_pair(
        std::make_shared<pipe<EndpointType>>(fd[0]),
        std::make_shared<pipe<EndpointType>>(fd[1])
    );
}

}} // namespace cocaine::io

#endif
