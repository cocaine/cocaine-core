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

#ifndef COCAINE_FORMAT_HPP
#define COCAINE_FORMAT_HPP

#include <boost/format.hpp>

namespace cocaine {

namespace detail {
    static inline
    std::string
    substitute(boost::format& message) {
        return message.str();
    }

    template<typename T, typename... Args>
    static inline
    std::string
    substitute(boost::format& message, const T& argument, const Args&... args) {
        return substitute(message % argument, args...);
    }
}

template<typename... Args>
static inline
std::string
format(const std::string& format, const Args&... args) {
    boost::format message(format);

    try {
        return detail::substitute(message, args...);
    } catch(const boost::io::format_error& e) {
        return "<unable to format the message>";
    }
}

} // namespace cocaine

#endif
