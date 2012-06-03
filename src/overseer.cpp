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

#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine;
using namespace cocaine::engine;

overseer_t::overseer_t(context_t& context, slave_config_t config):
    unique_id_t(config.uuid),
    m_context(context),
    m_bus(m_context.io(), ZMQ_DEALER, config.uuid)
{
    m_bus.connect(endpoint(config.app));
    
    m_watcher.set<overseer_t, &overseer_t::message>(this);
    m_watcher.start(m_bus.fd(), ev::READ);
    m_processor.set<overseer_t, &overseer_t::process>(this);
    m_pumper.set<overseer_t, &overseer_t::pump>(this);
    m_pumper.start(0.005f, 0.005f);

    m_heartbeat_timer.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);

    configure(config.app);
}

overseer_t::~overseer_t() { }

void overseer_t::configure(const std::string& app) {
    try {
        m_manifest.reset(new manifest_t(m_context, app));
        
        m_plugin = m_context.get<plugin_t>(
            m_manifest->type,
            category_traits<plugin_t>::args_type(*m_manifest)
        );
        
        m_suicide_timer.set<overseer_t, &overseer_t::timeout>(this);
        m_suicide_timer.start(m_manifest->policy.suicide_timeout);
    } catch(const configuration_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::server_error, e.what());
        send(packed);
        terminate();
    } catch(const registry_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::server_error, e.what());
        send(packed);
        terminate();
    } catch(const unrecoverable_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::server_error, e.what());
        send(packed);
        terminate();
    } catch(...) {
        rpc::packed<rpc::error> packed(
            dealer::server_error,
            "unexpected exception while configuring the slave"
        );
        
        send(packed);
        terminate();
    }
}

void overseer_t::run() {
    m_loop.loop();
}

blob_t overseer_t::recv(int timeout) {
    zmq::message_t message;

    networking::scoped_option<
        networking::options::receive_timeout
    > option(m_bus, timeout);

    m_bus.recv(&message);
    
    return blob_t(message.data(), message.size());
}

void overseer_t::message(ev::io&, int) {
    if(m_bus.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void overseer_t::process(ev::idle&, int) {
    if(!m_bus.pending()) {
        m_processor.stop();
        return;
    }
    
    int command = 0;

    m_bus.recv(command);

    switch(command) {
        case rpc::invoke: {
            // TEST: Ensure that we have the app first.
            BOOST_ASSERT(m_plugin.get() != NULL);

            std::string method;

            m_bus.recv(method);
            invoke(method);
            
            break;
        }
        
        case rpc::terminate:
            terminate();
            break;

        default:
            if(m_manifest.get() != 0) {
                m_manifest->log->warning(
                    "slave %s dropping unknown event type %d", 
                    id().c_str(),
                    command
                );
            }
            
            m_bus.drop();
    }

    // TEST: Ensure that we haven't missed something.
    BOOST_ASSERT(!m_bus.more());
}

void overseer_t::pump(ev::timer&, int) {
    message(m_watcher, ev::READ);
}

void overseer_t::timeout(ev::timer&, int) {
    rpc::packed<rpc::terminate> packed;
    send(packed);
    terminate();
}

void overseer_t::heartbeat(ev::timer&, int) {
    rpc::packed<rpc::heartbeat> packed;
    send(packed);
}

void overseer_t::invoke(const std::string& method) {
    try {
        io_t io(*this);
        m_plugin->invoke(method, io);
    } catch(const recoverable_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::app_error, e.what());
        send(packed);
    } catch(const unrecoverable_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::server_error, e.what());
        send(packed);
    } catch(...) {
        rpc::packed<rpc::error> packed(
            dealer::server_error,
            "unexpected exception while processing an event"
        );
        
        send(packed);
    }
    
    rpc::packed<rpc::choke> packed;
    send(packed);
    
    // NOTE: Drop all the outstanding request chunks not pulled
    // in by the user code. Might have a warning here?
    m_bus.drop();

    m_suicide_timer.stop();
    m_suicide_timer.start(m_manifest->policy.suicide_timeout);
}

void overseer_t::terminate() {
    m_loop.unloop(ev::ALL);
} 
