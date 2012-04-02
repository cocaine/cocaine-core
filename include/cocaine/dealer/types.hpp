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
    request_error  = 400,
    server_error   = 500,
    app_error      = 502,
    resource_error = 503,
    timeout_error  = 504,
    deadline_error = 520
};

}}

#endif // _COCAINE_DEALER_TYPES_HPP_INCLUDED_