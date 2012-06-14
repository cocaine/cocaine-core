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

#ifndef COCAINE_NATIVE_SERVER_JOB_HPP
#define COCAINE_NATIVE_SERVER_JOB_HPP

#include "cocaine/io.hpp"
#include "cocaine/job.hpp"

namespace cocaine { namespace engine { namespace drivers {

struct route {
    route(io::channel_t& channel_):
        channel(channel_)
    { }

    template<class T>
    void operator()(const T& route) {
        channel.send(io::protect(route), ZMQ_SNDMORE);
    }

    io::channel_t& channel;
};

class native_job_t:
    public job_t
{
    public:
        native_job_t(const std::string& event,
                     const blob_t& request,
                     const policy_t& policy,
                     io::channel_t& channel,
                     const io::route_t& route,
                     const std::string& tag);

        virtual void react(const events::chunk& event);
        virtual void react(const events::error& event);
        virtual void react(const events::choke& event);

    private:
        template<class Packed>
        void send(Packed& packed) {
            try {
                std::for_each(
                    m_route.begin(),
                    m_route.end(),
                    route(m_channel)
                );
            } catch(const zmq::error_t& e) {
                // NOTE: The client is down.
                return;
            }

            zmq::message_t null;

            m_channel.send(null, ZMQ_SNDMORE);          
            m_channel.send(m_tag, ZMQ_SNDMORE);
            m_channel.send_multi(packed);
        }

    private:
        io::channel_t& m_channel;        
        const io::route_t m_route;
        const std::string m_tag;
};

}}}

#endif
