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

#ifndef COCAINE_NATIVE_SERVER_DRIVER_HPP
#define COCAINE_NATIVE_SERVER_DRIVER_HPP

#include "cocaine/drivers/zeromq_server.hpp"

#include "cocaine/helpers/unique_id.hpp"

namespace cocaine { namespace engine { namespace drivers {

class lsd_server_t;

using helpers::unique_id_t;

class lsd_job_t:
    public unique_id_t,
    public job_t
{
    public:
        lsd_job_t(const unique_id_t::identifier_type& id,
                  lsd_server_t& driver,
                  const client::policy_t& policy,
                  const data_container_t& data,
                  const networking::route_t& route);

        virtual void react(const events::push_t& event);
        virtual void react(const events::error_t& event);
        virtual void react(const events::release_t& event);

    private:
        void send(const Json::Value& envelope, int flags = 0);

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

        // Driver interface.
        virtual Json::Value info() const;
        
    protected:
        // Server interface.
        virtual void process(ev::idle&, int);
};

}}}

#endif
