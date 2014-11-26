/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include <system_error>

namespace cocaine { namespace error {

enum dispatch_errors {
    service_error = 1,
    uncaught_error,
    resource_error,
    timeout_error,
    deadline_error
};

namespace aux {

class dispatch_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.rpc.dispatch";
    }

    virtual
    auto
    message(int code) const -> std::string {
        switch(code) {
          case dispatch_errors::uncaught_error:
            return "uncaught invocation exception";

          case dispatch_errors::resource_error:
            return "no resources available to complete invocation";

          case dispatch_errors::timeout_error:
            return "invocation has timed out";

          case dispatch_errors::deadline_error:
            return "invocation deadline has passed";
        }

        return "cocaine.rpc.dispatch error";
    }
};

} // namespace aux

inline
auto
dispatch_category() -> const std::error_category& {
    static aux::dispatch_category_t instance;
    return instance;
}

inline
auto
make_error_code(dispatch_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), dispatch_category());
}

} // namespace error

struct error_t:
    public std::exception
{
    template<typename... Args>
    error_t(const std::string& format, const Args&... args):
        m_message(cocaine::format(format, args...))
    { }

    virtual
   ~error_t() throw() {
        // Empty.
    }

    virtual
    auto
    what() const throw() -> const char* {
        return m_message.c_str();
    }

private:
    const std::string m_message;
};

} // namespace cocaine

namespace std {

template<>
struct is_error_code_enum<cocaine::error::dispatch_errors>:
    public true_type
{ };

} // namespace std

#endif
