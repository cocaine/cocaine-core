//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_MANIFEST_HPP
#define COCAINE_MANIFEST_HPP

#include "cocaine/common.hpp"
#include "cocaine/context.hpp"

namespace cocaine { namespace engine {

class manifest_t {
    public:
        manifest_t(context_t& context, 
                   const std::string& name, 
                   const Json::Value& manifest);

        std::string name;
        std::string type;
        std::string endpoint;

        Json::Value args;

        struct {
            float suicide_timeout;
            unsigned int pool_limit;
            unsigned int queue_limit;
        } policy;
};

}}

#endif
