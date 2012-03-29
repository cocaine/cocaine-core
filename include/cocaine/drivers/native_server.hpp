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

namespace cocaine { namespace engine { namespace drivers {

class native_server_t;

class native_job_t:
    public job_t
{
    public:
        native_job_t(native_server_t& driver,
                     const client::policy_t& policy,
                     const blob_t& request,
                     const networking::route_t& route,
                     const std::string& tag);

        virtual void react(const events::push_t& event);
        virtual void react(const events::error_t& event);
        virtual void react(const events::release_t& event);

    private:
        template<class Packed>
        void send(Packed& pack) {
            zeromq_server_t& server = static_cast<zeromq_server_t&>(m_driver);

            server.socket().send(m_tag, ZMQ_SNDMORE);
            server.socket().send_multi(pack.get());
        }

    private:
        const std::string m_tag;
        const networking::route_t m_route;
};

class native_server_t:
    public zeromq_server_t
{
    public:
        native_server_t(engine_t& engine,
                        const std::string& method, 
                        const Json::Value& args);

        // Driver interface.
        virtual Json::Value info() /* const */;
        
    private:
        typedef boost::tuple<
            std::string&,
            client::policy_t&,
            zmq::message_t*
        > request_proxy_t;

        // Server interface.
        virtual void process(ev::idle&, int);
};

}}}

#endif
