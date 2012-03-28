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

#include "cocaine/common.hpp"

namespace cocaine { namespace engine {

class app_t {
    public:
        app_t(context_t& ctx, const std::string& name);

        app_t(context_t& ctx, 
              const std::string& name, 
              const Json::Value& manifest);

    public:
        const std::string name;
        const Json::Value manifest;

        struct policy_t {
            float heartbeat_timeout;
            float suicide_timeout;
            unsigned int pool_limit;
            unsigned int queue_limit;
        } policy;

        boost::shared_ptr<logging::logger_t> log;

    private:
        void initialize(context_t& ctx);
};

class endpoint {
    public:
        endpoint(const std::string& name);
        operator std::string() const;

    private:
        std::string m_endpoint;
};

}}

#endif
