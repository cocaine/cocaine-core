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

struct message_path_t {
    message_path_t() {};
    message_path_t(const std::string& service_alias_,
                 const std::string& handle_name_) :
        service_alias(service_alias_),
        handle_name(handle_name_) {};

    message_path_t(const message_path_t& path) :
        service_alias(path.service_alias),
        handle_name(path.handle_name) {};

    message_path_t& operator = (const message_path_t& rhs) {
        if (this == &rhs) {
            return *this;
        }

        service_alias = rhs.service_alias;
        handle_name = rhs.handle_name;

        return *this;
    }

    bool operator == (const message_path_t& mp) const {
        return (service_alias == mp.service_alias &&
                handle_name == mp.handle_name);
    }

    bool operator != (const message_path_t& mp) const {
        return !(*this == mp);
    }

    std::string as_string() const {
        return "[" + service_alias + "." + handle_name + "]";
    }

    std::string service_alias;
    std::string handle_name;

    MSGPACK_DEFINE(service_alias, handle_name);
};

static std::size_t __attribute__ ((unused)) hash_value(const message_path_t& path) {
    boost::hash<std::string> hasher;
    return hasher(path.service_alias + path.handle_name);
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_MESSAGE_PATH_HPP_INCLUDED_
