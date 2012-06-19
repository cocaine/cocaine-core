/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

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
        uint16_t m_port;

        ev::io m_watcher; 
        ev::idle m_processor;

        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;
        
        networking::channel_t m_socket;
};

}}}

#endif
