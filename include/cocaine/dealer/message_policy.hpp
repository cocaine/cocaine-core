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

#ifndef _COCAINE_DEALER_MESSAGE_POLICY_HPP_INCLUDED_
#define _COCAINE_DEALER_MESSAGE_POLICY_HPP_INCLUDED_

#include <msgpack.hpp>

#include <string>
#include <sstream>

namespace cocaine {
namespace dealer {

struct message_policy {
    message_policy() :
        send_to_all_hosts(false),
        urgent(false),
        timeout(0.0f),
        deadline(0.0f),
        max_retries(0) {}

    message_policy(bool send_to_all_hosts,
                   bool urgent,
                   float mailboxed,
                   float timeout,
                   float deadline,
                   int max_retries) :
        send_to_all_hosts(send_to_all_hosts),
        urgent(urgent),
        timeout(timeout),
        deadline(deadline),
        max_retries(max_retries) {}

    message_policy(const message_policy& mp) {
        *this = mp;
    }

    message_policy& operator = (const message_policy& rhs) {
        if (this == &rhs) {
            return *this;
        }

        send_to_all_hosts = rhs.send_to_all_hosts;
        urgent = rhs.urgent;
        timeout = rhs.timeout;
        deadline = rhs.deadline;
        max_retries = rhs.max_retries;

        return *this;
    }

    bool operator == (const message_policy& rhs) const {
        return (send_to_all_hosts == rhs.send_to_all_hosts &&
                urgent == rhs.urgent &&
                timeout == rhs.timeout &&
                deadline == rhs.deadline);
    }

    bool operator != (const message_policy& rhs) const {
        return !(*this == rhs);
    }

    policy_t server_policy() const {
        return policy_t(urgent, timeout, deadline);
    }

    policy_t server_policy() {
        return policy_t(urgent, timeout, deadline);
    }

    bool send_to_all_hosts;
    bool urgent;
    double timeout;
    double deadline;
    int max_retries;

    MSGPACK_DEFINE(send_to_all_hosts,
                   urgent,
                   timeout,
                   deadline,
                   max_retries);
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_MESSAGE_POLICY_HPP_INCLUDED_
