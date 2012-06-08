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
#include "cocaine/manifest.hpp"

#include "cocaine/interfaces/driver.hpp"

#include "helpers/json.hpp"

namespace cocaine {

#if BOOST_VERSION >= 104000
typedef boost::ptr_unordered_map<
#else
typedef boost::ptr_map<
#endif
    const std::string,
    engine::drivers::driver_t
> driver_map_t;

class app_t {
    public:
        app_t(context_t& context,
              const std::string& name);
        
        ~app_t();

        void start();
        void stop();

        Json::Value info() const;
        
        // Job scheduling.
        void enqueue(const boost::shared_ptr<engine::job_t>& job);

    private:
        boost::shared_ptr<logging::logger_t> m_log;
        
        manifest_t m_manifest;
        std::auto_ptr<engine::engine_t> m_engine;
        
        driver_map_t m_drivers;
};

}

#endif
