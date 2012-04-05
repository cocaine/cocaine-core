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

#ifndef COCAINE_SLAVE_HPP
#define COCAINE_SLAVE_HPP

#include "cocaine/common.hpp"

#include "cocaine/context.hpp"
#include "cocaine/networking.hpp"

#include "cocaine/interfaces/plugin.hpp"

#include "cocaine/helpers/blob.hpp"
#include "cocaine/helpers/unique_id.hpp"

namespace cocaine { namespace engine {

class overseer_t:
    public boost::noncopyable,
    public unique_id_t
{
    public:
        overseer_t(const config_t& config);
        ~overseer_t();

        void run();

        blob_t recv(bool block);

        template<class Packed>
        void send(Packed& packed) {
            m_messages.send_multi(packed);
        }

    private:
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void timeout(ev::timer&, int);
        void heartbeat(ev::timer&, int);

        // Dispatching.
        void configure();
        void invoke(const std::string& method);
        void terminate();

    private:
        config_t m_config;
        
        // Runtime application context.
        // XXX: Initialize it in a better way.
        std::auto_ptr<context_t> m_context;
        std::auto_ptr<const app_t> m_app;
        std::auto_ptr<plugin_t> m_plugin;

        // Event loop.
        ev::default_loop m_loop;
        
        ev::io m_watcher;
        ev::idle m_processor;
        
        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        ev::timer m_heartbeat_timer, m_suicide_timer;
        
        // Engine RPC.
        zmq::context_t m_io;
        networking::channel_t m_messages;
};

}}

#endif
