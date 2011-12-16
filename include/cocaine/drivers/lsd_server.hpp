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

#ifndef COCAINE_DRIVER_LSD_SERVER_HPP
#define COCAINE_DRIVER_LSD_SERVER_HPP

#include "cocaine/drivers/zeromq_server.hpp"

namespace cocaine { namespace engine { namespace driver {

class lsd_server_t;

class lsd_job_t:
    public unique_id_t,
    public job::job_t
{
    public:
        lsd_job_t(const unique_id_t::type& id,
                  lsd_server_t& driver,
                  const client::policy_t& policy,
                  const networking::route_t& route);

        virtual void react(const events::chunk_t& event);
        virtual void react(const events::error_t& event);
        virtual void react(const events::choked_t& event);

    private:
        bool send(const Json::Value& envelope, int flags = 0);

    private:
        const networking::route_t m_route;
};

class lsd_server_t:
    public zeromq_server_t
{
    public:
        lsd_server_t(engine_t& engine,
                     const std::string& method, 
                     const Json::Value& args);

        // Driver interface
        virtual Json::Value info() const;
        
    private:
        // Server interface
        virtual void process(ev::idle&, int);
};

}}}

#endif
