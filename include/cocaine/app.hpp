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

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace engine {

class app_t {
    public:
        app_t(context_t& context,
              const std::string& name);

    public:
        const std::string& name() const {
            return m_name;
        }

        const std::string& type() const {
            return m_type;
        }

        Json::Value args() const {
            return m_manifest["args"];
        }

        Json::Value limits() const {
            return m_manifest["engine"]["resource-limits"];
        }

    public:
        struct {
            float heartbeat_timeout;
            float suicide_timeout;
            unsigned int pool_limit;
            unsigned int queue_limit;
            unsigned int grow_threshold;
        } policy;

        boost::shared_ptr<logging::logger_t> log;
        
    private:
        Json::Value m_manifest;
        
        std::string m_name;
        std::string m_type;
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
