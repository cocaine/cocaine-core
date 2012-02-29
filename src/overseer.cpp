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
#include "cocaine/registry.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine;
using namespace cocaine::engine;

overseer_t::overseer_t(const unique_id_t::type& id_, context_t& ctx, const app_t& app):
    unique_id_t(id_),
    object_t(ctx, app.name + " backend " + id()),
    m_app(app),
    m_messages(ctx, ZMQ_DEALER, id()),
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

overseer_t::~overseer_t() {
    m_module.reset();
}

void overseer_t::loop() {
    try {
        m_module = context().registry().create<plugin_t>(m_app.type);
        m_module->initialize(m_app);
    } catch(const unrecoverable_error_t& e) {
        m_messages.send_multi(
            boost::make_tuple(
                (const int)rpc::error,
                static_cast<const int>(client::server_error),
                std::string(e.what())
            )
        );

        return;
    } catch(const std::runtime_error& e) {
        m_messages.send_multi(
            boost::make_tuple(
                (const int)rpc::error,
                static_cast<const int>(client::server_error),
                std::string(e.what())
            )
        );

        return;
    } catch(...) {
        m_messages.send_multi(
            boost::make_tuple(
                (const int)rpc::error,
                static_cast<const int>(client::server_error),
                std::string("unexpected exception")
            )
        );

        return;
    }
        
    m_loop.loop();
}

void overseer_t::send(rpc::codes code, const void* data, size_t size) {
    const int command = code;
    zmq::message_t message(size);

    memcpy(message.data(), data, size);
    
    m_messages.send_multi(
        boost::tie(
            command,
            message
        )
    );
}

void overseer_t::message(ev::io&, int) {
    if(m_messages.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void overseer_t::process(ev::idle&, int) {
    if(m_messages.pending()) {
        unsigned int command = 0;

        m_messages.recv(command);

        switch(command) {
            case rpc::invoke: {
                std::string method;
                zmq::message_t request;
                boost::tuple<std::string&, zmq::message_t*> tier(method, &request);

                m_messages.recv_multi(tier);

                try {
                    invocation_site_t site(
                        *this,
                        request.data(),
                        request.size()
                    );

                    m_module->invoke(site, method);
                } catch(const recoverable_error_t& e) {
                    m_messages.send_multi(
                        boost::make_tuple(
                            (const int)rpc::error,
                            static_cast<const int>(client::app_error),
                            std::string(e.what())
                        )
                    );
                } catch(const unrecoverable_error_t& e) {
                    m_messages.send_multi(
                        boost::make_tuple(
                            (const int)rpc::error,
                            static_cast<const int>(client::server_error),
                            std::string(e.what())
                        )
                    );
                } catch(...) {
                    m_messages.send_multi(
                        boost::make_tuple(
                            (const int)rpc::error,
                            static_cast<const int>(client::server_error),
                            std::string("unexpected exception")
                        )
                    );
                }
                 
                m_messages.send((const int)rpc::release);

                m_suicide_timer.stop();
                m_suicide_timer.start(m_app.policy.suicide_timeout);
             
                break;
            }
            
            case rpc::terminate: {
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
    m_messages.send((const int)rpc::terminate);
    terminate();
}

void overseer_t::heartbeat(ev::timer&, int) {
    m_messages.send((const int)rpc::heartbeat);
}

void overseer_t::terminate() {
    m_loop.unloop();
} 

