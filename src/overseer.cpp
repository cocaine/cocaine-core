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

#include "cocaine/context.hpp"
#include "cocaine/dealer/types.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/registry.hpp"

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::plugin;

overseer_t::overseer_t(const unique_id_t::type& id_, context_t& context, app_t& app):
    unique_id_t(id_),
    m_context(context),
    m_app(app),
    m_messages(m_context, ZMQ_DEALER, id()),
    m_loop(),
    m_watcher(m_loop),
    m_processor(m_loop),
    m_pumper(m_loop),
    m_suicide_timer(m_loop),
    m_heartbeat_timer(m_loop)
{
    m_messages.connect(m_app.endpoint);
    
    m_watcher.set<overseer_t, &overseer_t::message>(this);
    m_watcher.start(m_messages.fd(), ev::READ);
    m_processor.set<overseer_t, &overseer_t::process>(this);
    m_pumper.set<overseer_t, &overseer_t::pump>(this);
    m_pumper.start(0.2f, 0.2f);

    m_suicide_timer.set<overseer_t, &overseer_t::timeout>(this);
    m_suicide_timer.start(m_app.policy.suicide_timeout);

    m_heartbeat_timer.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);
}

void overseer_t::loop() {
    try {
        m_module = m_context.registry().create<module_t>(m_app.type, m_app.args);
    } catch(const unrecoverable_error_t& e) {
        send(events::error_t(client::engine_error, e.what()));
        return;
    } catch(...) {
        send(events::error_t(client::engine_error, "unexpected exception"));
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
        unsigned int code = 0;

        m_messages.recv(code);

        switch(code) {
            case events::invoke_t::code: {
                events::invoke_t command;
                
                m_messages.recv(command);

                try {
                    m_module->invoke(
                        invocation_context_t(
                            *this,
                            command.method
                        )
                    );
                } catch(const recoverable_error_t& e) {
                    send(events::error_t(client::app_error, e.what()));
                } catch(const unrecoverable_error_t& e) {
                    send(events::error_t(client::engine_error, e.what())); 
                } catch(...) {
                    send(events::error_t(client::engine_error, "unexpected exception")); 
                }
                    
                send(events::release_t());

                m_suicide_timer.stop();
                m_suicide_timer.start(m_app.policy.suicide_timeout);
             
                break;
            }
            
            case events::terminate_t::code: {
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
    send(events::terminate_t());
    terminate();
}

void overseer_t::heartbeat(ev::timer&, int) {
    send(events::heartbeat_t());
}

void overseer_t::terminate() {
    m_loop.unloop();
} 

