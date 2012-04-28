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

#ifndef COCAINE_ZEROMQ_SERVER_DRIVER_HPP
#define COCAINE_ZEROMQ_SERVER_DRIVER_HPP

#include "cocaine/drivers/base.hpp"

#include "cocaine/job.hpp"

namespace cocaine { namespace engine { namespace drivers {

class zeromq_server_t;

class zeromq_server_job_t:
    public job_t
{
    public:
        zeromq_server_job_t(zeromq_server_t& driver,
                            const blob_t& request,
                            const networking::route_t& route);

        virtual void react(const events::push_t& event);

    private:
        const networking::route_t m_route;
};

class zeromq_server_t:
    public driver_t
{
    public:
        zeromq_server_t(engine_t& engine,
                        const std::string& method, 
                        const Json::Value& args,
                        int type = ZMQ_ROUTER);

        virtual ~zeromq_server_t();

        // Driver interface.
        virtual Json::Value info() /* const */;

    public:
        networking::channel_t& socket() {
            return m_socket;
        }

    protected:
        // Server interface.
        virtual void process(ev::idle&, int);

    private:
        void event(ev::io&, int);
        void pump(ev::timer&, int);

    protected:
        uint64_t m_backlog;
        int m_linger;
        uint8_t m_port;

        ev::io m_watcher; 
        ev::idle m_processor;

        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;
        
        networking::channel_t m_socket;
};

}}}

#endif
