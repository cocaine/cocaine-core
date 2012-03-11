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

#ifndef COCAINE_GENERIC_SLAVE_BACKEND_HPP
#define COCAINE_GENERIC_SLAVE_BACKEND_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/object.hpp"

#include "cocaine/helpers/data_container.hpp"
#include "cocaine/helpers/unique_id.hpp"
#include "cocaine/interfaces/plugin.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/rpc.hpp"

namespace cocaine { namespace engine {

using helpers::data_container_t;
using helpers::unique_id_t;

class overseer_t:
    public unique_id_t,
    public object_t
{
    public:
        overseer_t(const unique_id_t::identifier_type& id,
                   context_t& ctx,
                   const app_t& app);

        ~overseer_t();

        void run();

        data_container_t recv(bool block);

        template<class Pack>
        void send(Pack& pack) {
            m_messages.send_multi(pack.get());
        }

    private:
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void timeout(ev::timer&, int);
        void heartbeat(ev::timer&, int);

        void terminate();

    private:
        const app_t& m_app;

        // Engine RPC.
        networking::channel_t m_messages;

        // Application instance.
        std::auto_ptr<plugin_t> m_module;

        // Event loop.
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
