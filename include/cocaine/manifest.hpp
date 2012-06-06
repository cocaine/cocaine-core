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

#ifndef COCAINE_APP_MANIFEST_HPP
#define COCAINE_APP_MANIFEST_HPP

#include "cocaine/common.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace engine {

struct manifest_t {
    manifest_t(context_t& context,
               const std::string& app);

    std::string name,
                type,
                source;

    struct {
        float heartbeat_timeout;
        float suicide_timeout;
        unsigned int pool_limit;
        unsigned int queue_limit;
        unsigned int grow_threshold;
    } policy;

    // Path to a binary which will be used as a slave.
    std::string slave;

    // Manifest root object.
    Json::Value root;
};

}}

#endif
