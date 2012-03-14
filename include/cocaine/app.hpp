//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_APP_HPP
#define COCAINE_APP_HPP

#include <msgpack.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine {

struct app_t {
    app_t(context_t& ctx, 
          const std::string& name, 
          const Json::Value& manifest);

    std::string name;
    std::string type;
    std::string endpoint;

    Json::Value manifest;

    struct policy_t {
        float heartbeat_timeout;
        float suicide_timeout;
        unsigned int pool_limit;
        unsigned int queue_limit;

        MSGPACK_DEFINE(heartbeat_timeout, suicide_timeout, pool_limit, queue_limit);
    } policy;

    boost::shared_ptr<logging::logger_t> log;

    MSGPACK_DEFINE(name, type, endpoint, policy);
};

}}

#endif
