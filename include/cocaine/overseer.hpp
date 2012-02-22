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

#ifndef COCAINE_OVERSEER_HPP
#define COCAINE_OVERSEER_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace engine {

class overseer_t:
    public boost::noncopyable,
    public unique_id_t
{
    public:
        overseer_t(const unique_id_t::type& id,
                   context_t& context,
                   app_t& app);

        // Entry point 
        void loop();

        template<class T>
        bool send(const T& message) {
            return m_messages.send_multi(
                boost::tie(
                    message.type,
                    message
                )
            );
        }

    private:
        // Event loop callback handling and dispatching
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void timeout(ev::timer&, int);
        void heartbeat(ev::timer&, int);

        void terminate();

    private:
        context_t& m_context;

        // Messaging
        networking::channel_t m_messages;

        // Application instance
        app_t& m_app;
        boost::shared_ptr<plugin::module_t> m_module;

        // Event loop
        ev::dynamic_loop m_loop;
        
        ev::io m_watcher;
        ev::idle m_processor;
        
        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        ev::timer m_suicide_timer, m_heartbeat_timer;
};

}}

#endif
