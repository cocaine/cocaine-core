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

#include <cerrno>
#include <cstring>
#include <stdexcept>

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

struct configuration_error_t:
    public std::runtime_error
{
    configuration_error_t(const std::string& what):
        std::runtime_error(what)
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
