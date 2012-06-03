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

struct slave_config_t {
    std::string app;
    std::string uuid;
};

class overseer_t:
    public boost::noncopyable,
    public unique_id_t
{
    public:
        overseer_t(context_t& context,
                   slave_config_t config);

        ~overseer_t();

        void configure(const std::string& app);
        void run();

        blob_t recv(int timeout);

        template<class Packed>
        void send(Packed& packed) {
            m_bus.send_multi(packed);
        }

    private:
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void timeout(ev::timer&, int);
        void heartbeat(ev::timer&, int);

        // Dispatching.
        void invoke(const std::string& method);
        void terminate();

    private:
        // Runtime application context.
        context_t& m_context;
        
        std::auto_ptr<const manifest_t> m_manifest;
        std::auto_ptr<plugin_t> m_plugin;

        // Event loop.
        ev::default_loop m_loop;
        
        ev::io m_watcher;
        ev::idle m_processor;
        
        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        ev::timer m_heartbeat_timer,
                  m_suicide_timer;
        
        // Engine RPC.
        networking::channel_t m_bus;
};

}}

#endif
