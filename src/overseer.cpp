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

#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>

#include "cocaine/context.hpp"
#include "cocaine/client/types.hpp"
#include "cocaine/events.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/rpc.hpp"

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::plugin;

overseer_t::overseer_t(const unique_id_t::type& id_, context_t& context, const std::string& name):
    unique_id_t(id_),
    identifiable_t((boost::format("slave [%1%:%2%]") % name % id_).str()),
    m_context(context),
    m_messages(*m_context.io, ZMQ_DEALER, id()),
    m_loop(),
    m_watcher(m_loop),
    m_processor(m_loop),
    m_suicide_timer(m_loop),
    m_heartbeat_timer(m_loop)
{
    m_messages.connect(boost::algorithm::join(
        boost::assign::list_of
            (std::string("ipc:///var/run/cocaine"))
            (m_context.config.core.instance)
            (name),
        "/")
    );
    
    m_watcher.set<overseer_t, &overseer_t::message>(this);
    m_watcher.start(m_messages.fd(), ev::READ);
    m_processor.set<overseer_t, &overseer_t::process>(this);
    m_pumper.set<overseer_t, &overseer_t::pump>(this);
    m_pumper.start(0.2f, 0.2f);

    m_suicide_timer.set<overseer_t, &overseer_t::timeout>(this);
    m_suicide_timer.start(m_context.config.engine.suicide_timeout);

    m_heartbeat_timer.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);
}

void overseer_t::operator()(const std::string& type, const std::string& args) {
    try {
        m_app = core::registry_t::instance(m_context)->create(type, args);
    } catch(const unrecoverable_error_t& e) {
        syslog(LOG_ERR, "%s: unable to instantiate the app - %s", identity(), e.what());
        BOOST_VERIFY(send(rpc::error_t(client::server_error, e.what())));
        return;
    } catch(...) {
        syslog(LOG_ERR, "%s: caught an unexpected exception", identity());
        BOOST_VERIFY(send(rpc::error_t(client::server_error, "unexpected exception")));
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

        BOOST_VERIFY(m_messages.recv(code));

        switch(code) {
            case rpc::invoke: {
                rpc::invoke_t object;
                zmq::message_t request;
                boost::tuple<rpc::invoke_t&, zmq::message_t*> tier(object, &request);
                
                BOOST_VERIFY(m_messages.recv_multi(tier));
                BOOST_ASSERT(object.type == rpc::invoke);

                try {
                    m_app->invoke(
                        boost::bind(&overseer_t::respond, this, _1, _2),
                        object.method, 
                        request.data(), 
                        request.size());
#if BOOST_VERSION >= 103500
                    boost::this_thread::interruption_point();
#endif
                } catch(const recoverable_error_t& e) {
                    syslog(LOG_ERR, "%s: '%s' invocation failed - %s", 
                        identity(), object.method.c_str(), e.what());
                    BOOST_VERIFY(send(rpc::error_t(client::app_error, e.what())));
                } catch(const unrecoverable_error_t& e) {
                    syslog(LOG_ERR, "%s: '%s' invocation failed - %s", 
                        identity(), object.method.c_str(), e.what());
                    BOOST_VERIFY(send(rpc::error_t(client::server_error, e.what()))); 
                } catch(...) {
                    syslog(LOG_ERR, "%s: caught an unexpected exception", identity());
                    BOOST_VERIFY(send(rpc::error_t(client::server_error, "unexpected exception"))); 
                }
                    
                BOOST_VERIFY(send(rpc::choke_t()));

                m_suicide_timer.stop();
                m_suicide_timer.start(m_context.config.engine.suicide_timeout);
             
                break;
            }
            
            case rpc::terminate: {
                rpc::terminate_t object;

                BOOST_VERIFY(m_messages.recv(object));
                BOOST_ASSERT(object.type == rpc::terminate);

                terminate();

                return;
            }
        }
    } else {
        m_processor.stop();
    }
}

void overseer_t::pump(ev::timer&, int) {
    message(m_watcher, ev::READ);
}

void overseer_t::respond(const void* response, size_t size) {
#if BOOST_VERSION >= 103500
    boost::this_thread::interruption_point();
#endif

    zmq::message_t message(size);
    memcpy(message.data(), response, size);
  
    BOOST_VERIFY(send(rpc::chunk_t(), ZMQ_SNDMORE) && m_messages.send(message));
}

void overseer_t::timeout(ev::timer&, int) {
    if(m_messages.pending()) {
        syslog(LOG_ERR, "%s: postponing the suicide", identity());
        m_suicide_timer.start(m_context.config.engine.suicide_timeout);
        return;
    }
    
    BOOST_VERIFY(send(rpc::terminate_t()));
    terminate();
}

void overseer_t::heartbeat(ev::timer&, int) {
    BOOST_VERIFY(send(rpc::heartbeat_t()));
}

void overseer_t::terminate() {
    m_loop.unloop();
} 

