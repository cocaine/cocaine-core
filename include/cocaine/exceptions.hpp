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

#ifndef COCAINE_EXCEPTIONS_HPP
#define COCAINE_EXCEPTIONS_HPP

#include "cocaine/format.hpp"

#include <cstring>
#include <exception>

namespace cocaine {

enum error_code {
    invocation_error = 1,
    resource_error,
    timeout_error,
    deadline_error
};

struct error_t:
    public std::exception
{
    template<typename... Args>
    error_t(const std::string& format,
            const Args&... args):
        m_message(cocaine::format(format, args...))
    { }

    virtual
    ~error_t() throw() {
        // Empty.
    }

    virtual
    const char*
    what() const throw() {
        return m_message.c_str();
    }

private:
    const std::string m_message;
};

struct configuration_error_t:
    public error_t
{
    template<typename... Args>
    configuration_error_t(const std::string& format,
                          const Args&... args):
        error_t(format, args...)
    { }
};

struct io_error_t:
    public cocaine::error_t
{
    template<typename... Args>
    io_error_t(const std::string& format, Args&&... args):
        cocaine::error_t(format, std::forward<Args>(args)...)
    {
#ifdef _GNU_SOURCE
        m_message = ::strerror_r(errno, m_buffer, 256);
#else
        ::strerror_r(errno, m_buffer, 256);

        // NOTE: XSI-compliant strerror_r() returns int instead of the
        // string buffer, so complete the job manually.
        m_message = m_buffer;
#endif
    }

    const char *
    describe() const throw() {
        return m_message;
    }

private:
    char m_buffer[256];
    const char * m_message;
};

} // namespace cocaine

#endif
