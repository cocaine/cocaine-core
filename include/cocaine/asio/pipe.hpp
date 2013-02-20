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

#ifndef COCAINE_ASIO_PIPE_HPP
#define COCAINE_ASIO_PIPE_HPP

#include "cocaine/common.hpp"

#include <fcntl.h>
#include <sys/socket.h>

namespace cocaine { namespace io {

struct pipe_t:
    boost::noncopyable
{
    template<class T>
    explicit
    pipe_t(T endpoint):
        m_fd(endpoint.create())
    {
        if(m_fd == -1) {
            throw io_error_t("unable to create a stream");
        }

        if(::connect(m_fd, endpoint.data(), endpoint.size()) == -1) {
            throw io_error_t("unable to connect a stream to '%s'", endpoint);
        }

        ::fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(m_fd, F_SETFL, O_NONBLOCK);
    }

    pipe_t(int fd):
        m_fd(fd)
    { }

    ~pipe_t();

    // Moving

    pipe_t(pipe_t&& other);

    pipe_t&
    operator=(pipe_t&& other);

    // Operations

    ssize_t
    write(const char * buffer,
          size_t size);

    ssize_t
    read(char * buffer,
         size_t size);

public:
    int
    fd() const {
        return m_fd;
    }

private:
    int m_fd;
};

}} // namespace cocaine::io

#endif
