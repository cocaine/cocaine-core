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

#include <boost/format.hpp>
#include <cerrno>
#include <exception>

namespace cocaine {

enum error_code {
    request_error   = 400,
    location_error  = 404,
    server_error    = 500,
    app_error       = 502,
    resource_error  = 503,
    timeout_error   = 504,
    deadline_error  = 520
};

struct error_t:
    public std::exception
{
    template<typename... Args>
    error_t(const std::string& format,
            const Args&... args)
    {
        boost::format message(format);

        try {
            // Recursively expand the argument pack.
            substitute(message, args...);
        } catch(const boost::io::format_error& e) {
            m_message = "<unable to format the message>";
            return;
        }

        m_message = message.str();
    }

    virtual
    const char *
    what() const throw() {
        return m_message.c_str();
    }

private:
    template<typename T, typename... Args>
    void
    substitute(boost::format& message,
               const T& argument,
               const Args&... args) const
    {
        substitute(message % argument, args...);
    }

    void
    substitute(boost::format&) const {
        return;
    }

private:
    std::string m_message;
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

struct system_error_t:
    public std::runtime_error
{
    public:
        system_error_t(const std::string& what):
            std::runtime_error(what)
        {
            ::strerror_r(errno, m_reason, 1024);
        }

        const char*
        reason() const {
            return m_reason;
        }

    private:
        char m_reason[1024];
};

} // namespace cocaine

#endif
