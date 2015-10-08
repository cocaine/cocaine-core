/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

namespace cocaine { namespace aux {

static inline
std::string
substitute(boost::format&& message) {
    return message.str();
}

template<typename T, class... Args>
static inline
std::string
substitute(boost::format&& message, T&& argument, Args&&... args) {
    return substitute(std::move(message % std::forward<T>(argument)), std::forward<Args>(args)...);
}

} // namespace aux

template<class... Args>
static inline
std::string
format(const std::string& format, Args&&... args) {
    try {
        return aux::substitute(std::move(boost::format(format)), std::forward<Args>(args)...);
    } catch(const boost::io::format_error& e) {
        return aux::substitute(std::move(boost::format("<invalid string format - %s>")), e.what());
    }
}

} // namespace cocaine

#endif
