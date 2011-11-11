#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>
#include <boost/thread.hpp>

#include "cocaine/overseer.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/registry.hpp"

using namespace cocaine::engine;
using namespace cocaine::plugin;

overseer_t::overseer_t(unique_id_t::reference id_, zmq::context_t& context, const std::string& name):
    unique_id_t(id_),
    m_context(context),
    m_messages(m_context, ZMQ_DEALER, id()),
    m_loop(),
    m_watcher(m_loop),
    m_processor(m_loop),
    m_suicide_timer(m_loop),
    m_heartbeat_timer(m_loop)
{
    m_messages.connect(boost::algorithm::join(
        boost::assign::list_of
            (std::string("ipc:///var/run/cocaine"))
            (config_t::get().core.instance)
            (name),
        "/"));
    
    m_watcher.set<overseer_t, &overseer_t::message>(this);
    m_watcher.start(m_messages.fd(), ev::READ);
    m_processor.set<overseer_t, &overseer_t::process>(this);
    m_processor.start();

    m_suicide_timer.set<overseer_t, &overseer_t::timeout>(this);
    m_suicide_timer.start(config_t::get().engine.suicide_timeout);

    m_heartbeat_timer.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0, 5.0);
}

overseer_t::~overseer_t() {
    terminate();
}

void overseer_t::operator()(const std::string& type, const std::string& args) {
    try {
        m_app = core::registry_t::instance()->create(type, args);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "worker [%s]: unable to instantiate the app - %s", 
            id().c_str(), e.what());
        m_messages.send(TERMINATE);
        return;
    }
        
    m_loop.loop();
}

void overseer_t::message(ev::io&, int) {
    if(m_messages.pending()) {
        m_watcher.stop();
        m_processor.start();
    }
}

void overseer_t::process(ev::idle&, int) {
    if(m_messages.pending()) {
        unsigned int code = 0;

        m_messages.recv(code);

        switch(code) {
            case INVOKE: {
                std::string method;
                zmq::message_t request;

                m_messages.recv(method);

                if(m_messages.more()) {
                    m_messages.recv(&request);
                }

                // Audit
                ev::tstamp start = m_loop.now();

                try {
                    m_app->invoke(
                        boost::bind(&overseer_t::respond, this, _1, _2),
                        method, 
                        request.data(), 
                        request.size());
                } catch(const std::exception& e) {
                    syslog(LOG_ERR, "worker [%s]: '%s' invocation failed - %s", 
                        id().c_str(), method.c_str(), e.what());
                    
                    boost::this_thread::interruption_point();
                   
                    m_messages.send_multi(
                        boost::make_tuple(
                            ERROR,
                            std::string(e.what())));
                }
                    
                boost::this_thread::interruption_point();

                ev_now_update(m_loop);
                
                m_messages.send_multi(
                    boost::make_tuple(
                        CHOKE,
                        m_loop.now() - start));
                
                m_suicide_timer.stop();
                m_suicide_timer.start(config_t::get().engine.suicide_timeout);
                
                break;
            }
            
            case TERMINATE:
                terminate();
                return;
        }
    } else {
        m_watcher.start(m_messages.fd(), ev::READ);
        m_processor.stop();
    }
}

void overseer_t::respond(const void* response, size_t size) {
    boost::this_thread::interruption_point();
    
    zmq::message_t message(size);
    memcpy(message.data(), response, size);
   
    m_messages.send_multi(
        boost::make_tuple(
            CHUNK,
            boost::ref(message)));
}

void overseer_t::timeout(ev::timer&, int) {
    if(m_messages.pending()) {
        return;
    }
    
    m_messages.send(SUICIDE);
    terminate();
}

void overseer_t::heartbeat(ev::timer&, int) {
    m_messages.send(HEARTBEAT);
}

void overseer_t::terminate() {
    m_loop.unloop();
} 

