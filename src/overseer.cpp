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

#include "cocaine/overseer.hpp"

#include "cocaine/context.hpp"

#include "cocaine/registry.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine;
using namespace cocaine::engine;

overseer_t::overseer_t(context_t& ctx,
                       const unique_id_t::identifier_type& id_,
                       const std::string& app):
    object_t(ctx),
    unique_id_t(id_),
    m_app(ctx, app),
    m_loop(),
    m_watcher(m_loop),
    m_processor(m_loop),
    m_pumper(m_loop),
    m_suicide_timer(m_loop),
    m_heartbeat_timer(m_loop),
    m_messages(ctx, ZMQ_DEALER, id())
{
    m_messages.connect(endpoint(app));
    
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

overseer_t::~overseer_t() {
    m_module.reset();
}

void overseer_t::run() {
    try {
        std::string type = m_app.manifest["type"].asString();

        if(!type.empty()) {
            m_module = context().create<plugin_t>(type);
            m_module->initialize(m_app);
        } else {
            throw configuration_error_t("no app type has been specified");
        }
    } catch(const configuration_error_t& e) {
        events::error_t event(e);
        rpc::packed<events::error_t> packed(event);
        send(packed);
        return;
    } catch(const registry_error_t& e) {
        events::error_t event(e);
        rpc::packed<events::error_t> packed(event);
        send(packed);
        return;
    } catch(const unrecoverable_error_t& e) {
        events::error_t event(e);
        rpc::packed<events::error_t> packed(event);
        send(packed);
        return;
    } catch(...) {
        rpc::packed<events::error_t> packed(
            events::error_t(
                client::server_error,
                "unexpected exception while creating the plugin instance"
            )
        );
        
        send(packed);
        return;
    }

    m_loop.loop();
}

blob_t overseer_t::recv(bool block) {
    zmq::message_t message;
    
    m_messages.recv(&message, block ? 0 : ZMQ_NOBLOCK);
    
    return blob_t(message.data(), message.size());
}

void overseer_t::message(ev::io&, int) {
    if(m_messages.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void overseer_t::process(ev::idle&, int) {
    if(!m_messages.pending()) {
        m_processor.stop();
        return;
    }
    
    int command = 0;

    m_messages.recv(command);

    switch(command) {
        // case rpc::configure: {
        //     m_messages.recv(context().config);
        //     break;
        // }

        case rpc::invoke: {
            std::string method;

            m_messages.recv(method);

            try {
                io_t io(*this);
                m_module->invoke(io, method);
            } catch(const recoverable_error_t& e) {
                events::error_t event(e);
                rpc::packed<events::error_t> packed(event);
                send(packed);
            } catch(const unrecoverable_error_t& e) {
                events::error_t event(e);
                rpc::packed<events::error_t> packed(event);
                send(packed);
            } catch(...) {
                rpc::packed<events::error_t> packed(
                    events::error_t(
                        client::server_error,
                        "unexpected exception while invoking a method"
                    )
                );
                
                send(packed);
            }
            
            rpc::packed<events::release_t> packed;
            send(packed);
            
            // NOTE: Drop all the outstanding request chunks not pulled
            // in by the user code. Might have a warning here?
            m_messages.drop_remaining_parts();

            m_suicide_timer.stop();
            m_suicide_timer.start(m_app.policy.suicide_timeout);
         
            break;
        }
        
        case rpc::terminate:
            terminate();
            break;

        default:
            m_app.log->warning(
                "slave %s dropping unknown event type %d", 
                id().c_str(),
                command
            );
            
            m_messages.drop_remaining_parts();
    }

    BOOST_ASSERT(!m_messages.more());
}

void overseer_t::pump(ev::timer&, int) {
    message(m_watcher, ev::READ);
}

void overseer_t::timeout(ev::timer&, int) {
    rpc::packed<events::terminate_t> packed;
    send(packed);
    terminate();
}

void overseer_t::heartbeat(ev::timer&, int) {
    rpc::packed<events::heartbeat_t> packed;
    send(packed);
}

void overseer_t::terminate() {
    m_loop.unloop();
} 

