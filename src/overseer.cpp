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

#include "cocaine/overseer.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/registry.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::plugin;

overseer_t::overseer_t(const unique_id_t::type& id_, context_t& context, manifest_t& manifest):
    unique_id_t(id_),
    m_context(context),
    m_manifest(manifest),
    m_messages(m_context, ZMQ_DEALER, id()),
    m_loop(),
    m_watcher(m_loop),
    m_processor(m_loop),
    m_pumper(m_loop),
    m_suicide_timer(m_loop),
    m_heartbeat_timer(m_loop)
{
    m_messages.connect(m_manifest.endpoint);
    
    m_watcher.set<overseer_t, &overseer_t::message>(this);
    m_watcher.start(m_messages.fd(), ev::READ);
    m_processor.set<overseer_t, &overseer_t::process>(this);
    m_pumper.set<overseer_t, &overseer_t::pump>(this);
    m_pumper.start(0.2f, 0.2f);

    m_suicide_timer.set<overseer_t, &overseer_t::timeout>(this);
    m_suicide_timer.start(m_manifest.policy.suicide_timeout);

    m_heartbeat_timer.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);
}

void overseer_t::loop() {
    try {
        m_module = m_context.registry().create<module_t>(m_manifest);
    } catch(const unrecoverable_error_t& e) {
        m_messages.send_multi(
            boost::make_tuple(
                4,
                static_cast<int>(client::server_error),
                std::string(e.what())
            )
        );

        return;
    } catch(...) {
        m_messages.send_multi(
            boost::make_tuple(
                4,
                static_cast<int>(client::server_error),
                std::string("unexpected exception")
            )
        );

        return;
    }
        
    m_loop.loop();
}

void overseer_t::message(ev::io&, int) {
    if(m_messages.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void overseer_t::process(ev::idle&, int) {
    if(m_messages.pending()) {
        unsigned int type = 0;

        m_messages.recv(type);

        switch(type) {
            case 1: {
                std::string method;
                zmq::message_t request;
                boost::tuple<std::string&, zmq::message_t*> tier(method, &request);

                m_messages.recv_multi(tier);

                try {
                    invocation_context_t context;
                    m_module->invoke(context);
                } catch(const recoverable_error_t& e) {
                    m_messages.send_multi(
                        boost::make_tuple(
                            4,
                            static_cast<int>(client::app_error),
                            std::string(e.what())
                        )
                    );
                } catch(const unrecoverable_error_t& e) {
                    m_messages.send_multi(
                        boost::make_tuple(
                            4,
                            static_cast<int>(client::server_error),
                            std::string(e.what())
                        )
                    );
                } catch(...) {
                    m_messages.send_multi(
                        boost::make_tuple(
                            4,
                            static_cast<int>(client::server_error),
                            std::string("unexpected exception")
                        )
                    );
                }
                    
                m_messages.send(5);

                m_suicide_timer.stop();
                m_suicide_timer.start(m_manifest.policy.suicide_timeout);
             
                break;
            }
            
            case 2: {
                terminate();
            }
        }
    } else {
        m_processor.stop();
    }
}

void overseer_t::pump(ev::timer&, int) {
    message(m_watcher, ev::READ);
}

void overseer_t::timeout(ev::timer&, int) {
    m_messages.send(6);
    terminate();
}

void overseer_t::heartbeat(ev::timer&, int) {
    m_messages.send(0);
}

void overseer_t::terminate() {
    m_loop.unloop();
} 

