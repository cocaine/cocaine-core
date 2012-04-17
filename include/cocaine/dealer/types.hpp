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

#ifndef _COCAINE_DEALER_TYPES_HPP_INCLUDED_
#define _COCAINE_DEALER_TYPES_HPP_INCLUDED_

#include <msgpack.hpp>

#include "cocaine/common.hpp"

namespace cocaine {
namespace dealer {

struct policy_t {
    policy_t():
        urgent(false),
        timeout(0.0f),
        deadline(0.0f)
    { }

    policy_t(bool urgent_, ev::tstamp timeout_, ev::tstamp deadline_):
        urgent(urgent_),
        timeout(timeout_),
        deadline(deadline_)
    { }

    bool urgent;
    ev::tstamp timeout;
    ev::tstamp deadline;

    MSGPACK_DEFINE(urgent, timeout, deadline);
};

enum error_code {
    request_error   = 400,
    location_error  = 404,
    server_error    = 500,
    app_error       = 502,
    resource_error  = 503,
    timeout_error   = 504,
    deadline_error  = 520
};

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

    size_t data_size() const {
        return (service_name.length() + handle_name.length());
    }

    std::string service_name;
    std::string handle_name;

    MSGPACK_DEFINE(service_name, handle_name);
};

struct message_policy {
    message_policy() :
        send_to_all_hosts(false),
        urgent(false),
        mailboxed(false),
        timeout(0.0f),
        deadline(0.0f),
        max_timeout_retries(0) {};

    message_policy(bool send_to_all_hosts_,
                   bool urgent_,
                   float mailboxed_,
                   float timeout_,
                   float deadline_,
                   int max_timeout_retries_) :
        send_to_all_hosts(send_to_all_hosts_),
        urgent(urgent_),
        mailboxed(mailboxed_),
        timeout(timeout_),
        deadline(deadline_),
        max_timeout_retries(max_timeout_retries_) {};

    message_policy(const message_policy& mp) {
        *this = mp;
    }

    message_policy& operator = (const message_policy& rhs) {
        if (this == &rhs) {
            return *this;
        }

        send_to_all_hosts = rhs.send_to_all_hosts;
        urgent = rhs.urgent;
        mailboxed = rhs.mailboxed;
        timeout = rhs.timeout;
        deadline = rhs.deadline;
        max_timeout_retries = rhs.max_timeout_retries;

        return *this;
    }

    bool operator == (const message_policy& rhs) const {
        return (send_to_all_hosts == rhs.send_to_all_hosts &&
                urgent == rhs.urgent &&
                mailboxed == rhs.mailboxed &&
                timeout == rhs.timeout &&
                deadline == rhs.deadline);
    }

    bool operator != (const message_policy& rhs) const {
        return !(*this == rhs);
    }

    policy_t server_policy() const {
        return policy_t(urgent, timeout, deadline);
    }

    bool send_to_all_hosts;
    bool urgent;
    bool mailboxed;
    double timeout;
    double deadline;
    int max_timeout_retries;

    MSGPACK_DEFINE(send_to_all_hosts,
                   urgent,
                   mailboxed,
                   timeout,
                   deadline,
                   max_timeout_retries);
};

}}

#endif // _COCAINE_DEALER_TYPES_HPP_INCLUDED_