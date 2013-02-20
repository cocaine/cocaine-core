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

#include "cocaine/common.hpp"

#include <fcntl.h>
#include <sys/socket.h>

namespace cocaine { namespace io {

struct acceptor_t:
    boost::noncopyable
{
    typedef pipe_t pipe_type;

    template<class T>
    acceptor_t(T endpoint, int backlog = 1024):
        m_fd(endpoint.create())
    {
        if(m_fd == -1) {
            throw io_error_t("unable to create an acceptor");
        }

        ::fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(m_fd, F_SETFL, O_NONBLOCK);

        if(::bind(m_fd, endpoint.data(), endpoint.size()) == -1) {
            throw io_error_t("unable to bind an acceptor on '%s'", endpoint);
        }

        ::listen(m_fd, backlog);
    }

    ~acceptor_t();

    // Moving

    acceptor_t(acceptor_t&& other);

    acceptor_t&
    operator=(acceptor_t&& other);

    // Operations

    std::shared_ptr<pipe_t>
    accept();

public:
    int
    fd() const {
        return m_fd;
    }

private:
    int m_fd;
};

typedef std::pair<
    std::shared_ptr<pipe_t>,
    std::shared_ptr<pipe_t>
> pipe_link_t;

pipe_link_t
link();

}} // namespace cocaine::io

#endif
