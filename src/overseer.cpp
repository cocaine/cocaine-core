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

#include "cocaine/app.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine;
using namespace cocaine::engine;

overseer_t::overseer_t(const config_t& config):
    unique_id_t(config.slave.id),
    m_config(config),
    m_io(1),
    m_messages(m_io, ZMQ_DEALER, id())
{
    m_messages.connect(endpoint(config.slave.app));
    
    m_watcher.set<overseer_t, &overseer_t::message>(this);
    m_watcher.start(m_messages.fd(), ev::READ);
    m_processor.set<overseer_t, &overseer_t::process>(this);
    m_pumper.set<overseer_t, &overseer_t::pump>(this);
    m_pumper.start(0.2f, 0.2f);

    m_heartbeat_timer.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);
}

overseer_t::~overseer_t() { }

void overseer_t::run() {
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
        case rpc::configure:
            // TEST: No slave reconfiguration is allowed, yet.
            BOOST_ASSERT(m_plugin.get() == NULL);
            
            m_messages.recv(m_config);
            configure();
            
            break;

        case rpc::invoke: {
            // TEST: Ensure that we have the application first.
            BOOST_ASSERT(m_plugin.get() != NULL);

            std::string method;

            m_messages.recv(method);
            invoke(method);
            
            break;
        }
        
        case rpc::terminate:
            terminate();
            break;

        default:
            if(m_app.get() != 0) {
                m_app->log->warning(
                    "slave %s dropping unknown event type %d", 
                    id().c_str(),
                    command
                );
            }
            
            m_messages.drop_remaining_parts();
    }

    // TEST: Ensure that we haven't missed something.
    BOOST_ASSERT(!m_messages.more());
}

void overseer_t::pump(ev::timer&, int) {
    message(m_watcher, ev::READ);
}

void overseer_t::timeout(ev::timer&, int) {
    terminate();
}

void overseer_t::heartbeat(ev::timer&, int) {
    rpc::packed<rpc::heartbeat> packed;
    send(packed);
}

void overseer_t::configure() {
    try {
        m_context.reset(new context_t(m_config));
        m_app.reset(new app_t(*m_context, m_config.slave.app));

        std::string type(m_app->manifest["type"].asString());

        if(!type.empty()) {
            m_plugin = m_context->create<plugin_t>(type);
            m_plugin->initialize(*m_app);
        } else {
            throw configuration_error_t("no app type has been specified");
        }
        
        m_suicide_timer.set<overseer_t, &overseer_t::timeout>(this);
        m_suicide_timer.start(m_app->policy.suicide_timeout);
    } catch(const configuration_error_t& e) {
        rpc::packed<rpc::error> packed(client::server_error, e.what());
        send(packed);
        terminate();
    } catch(const registry_error_t& e) {
        rpc::packed<rpc::error> packed(client::server_error, e.what());
        send(packed);
        terminate();
    } catch(const unrecoverable_error_t& e) {
        rpc::packed<rpc::error> packed(client::server_error, e.what());
        send(packed);
        terminate();
    } catch(...) {
        rpc::packed<rpc::error> packed(
            client::server_error,
            "unexpected exception while configuring the slave"
        );
        
        send(packed);
        terminate();
    }
}

void overseer_t::invoke(const std::string& method) {
    try {
        io_t io(*this);
        m_plugin->invoke(method, io);
    } catch(const recoverable_error_t& e) {
        rpc::packed<rpc::error> packed(client::app_error, e.what());
        send(packed);
    } catch(const unrecoverable_error_t& e) {
        rpc::packed<rpc::error> packed(client::server_error, e.what());
        send(packed);
    } catch(...) {
        rpc::packed<rpc::error> packed(
            client::server_error,
            "unexpected exception while invoking a method"
        );
        
        send(packed);
    }
    
    rpc::packed<rpc::release> packed;
    send(packed);
    
    // NOTE: Drop all the outstanding request chunks not pulled
    // in by the user code. Might have a warning here?
    m_messages.drop_remaining_parts();

    m_suicide_timer.stop();
    m_suicide_timer.start(m_app->policy.suicide_timeout);
}

void overseer_t::terminate() {
    m_loop.unloop();
} 
