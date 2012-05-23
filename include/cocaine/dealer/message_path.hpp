//
// Copyright (C) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
