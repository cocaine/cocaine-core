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

#ifndef COCAINE_DRIVER_NATIVE_SERVER_HPP
#define COCAINE_DRIVER_NATIVE_SERVER_HPP

#include "cocaine/drivers/zeromq_server.hpp"

namespace cocaine { namespace engine { namespace driver {

class native_server_t;

class native_server_job_t:
    public unique_id_t,
    public job::job_t
{
    public:
        native_server_job_t(native_server_t& driver,
                            const client::policy_t& policy,
                            const unique_id_t::type& id,
                            const networking::route_t& route);

        virtual void react(const events::push_t& event);
        virtual void react(const events::error_t& event);
        virtual void react(const events::release_t& event);

    private:
        template<class T>
        void send(const T& response, int flags = 0) {
            zmq::message_t message;
            zeromq_server_t& server = static_cast<zeromq_server_t&>(m_driver);

            // Send the identity
            for(networking::route_t::const_iterator id = m_route.begin(); id != m_route.end(); ++id) {
                message.rebuild(id->size());
                memcpy(message.data(), id->data(), id->size());
                server.socket().send(message, ZMQ_SNDMORE);
            }

            // Send the delimiter
            message.rebuild(0);
            server.socket().send(message, ZMQ_SNDMORE);

            // Send the response
            server.socket().send_multi(
                boost::tie(
                    response.type,
                    response
                ),
                flags
            );
        }

    private:
        const networking::route_t m_route;
};

class native_server_t:
    public zeromq_server_t
{
    public:
        native_server_t(engine_t& engine,
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
