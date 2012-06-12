/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
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

#ifndef _COCAINE_DEALER_MESSAGE_PATH_HPP_INCLUDED_
#define _COCAINE_DEALER_MESSAGE_PATH_HPP_INCLUDED_

#include <msgpack.hpp>

#include <string>

namespace cocaine {
namespace dealer {

struct message_path {
    message_path() {};
    message_path(const std::string& service_name_,
                 const std::string& handle_name_) :
        service_name(service_name_),
        handle_name(handle_name_) {};

    message_path(const message_path& path) :
        service_name(path.service_name),
        handle_name(path.handle_name) {};

    message_path& operator = (const message_path& rhs) {
        if (this == &rhs) {
            return *this;
        }

        service_name = rhs.service_name;
        handle_name = rhs.handle_name;

        return *this;
    }

    bool operator == (const message_path& mp) const {
        return (service_name == mp.service_name &&
                handle_name == mp.handle_name);
    }

    bool operator != (const message_path& mp) const {
        return !(*this == mp);
    }

    std::string service_name;
    std::string handle_name;

    MSGPACK_DEFINE(service_name, handle_name);
};

static std::size_t __attribute__ ((unused)) hash_value(const message_path& path) {
    boost::hash<std::string> hasher;
    return hasher(path.service_name + path.handle_name);
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_MESSAGE_PATH_HPP_INCLUDED_
