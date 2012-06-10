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

#ifndef COCAINE_NATIVE_SERVER_JOB_HPP
#define COCAINE_NATIVE_SERVER_JOB_HPP

#include "cocaine/io.hpp"
#include "cocaine/job.hpp"

namespace cocaine { namespace engine { namespace drivers {

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
                    io::route(m_channel)
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
