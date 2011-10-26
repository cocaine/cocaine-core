#include <boost/thread.hpp>

#include "cocaine/overseer.hpp"
#include "cocaine/plugin.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::drivers;
using namespace cocaine::plugin;

overseer_t::overseer_t(unique_id_t::reference id_, zmq::context_t& context, const std::string& name):
    unique_id_t(id_),
    m_context(context),
    m_messages(m_context, ZMQ_DEALER, id()),
    m_loop(),
    m_message_watcher(m_loop),
    m_message_processor(m_loop),
    m_suicide_timer(m_loop),
    m_heartbeat_timer(m_loop)
{
    m_messages.connect("ipc:///var/run/cocaine/engines/" + name);
    m_message_watcher.set<overseer_t, &overseer_t::message>(this);
    m_message_watcher.start(m_messages.fd(), ev::READ);
        
    m_message_processor.set<overseer_t, &overseer_t::process_message>(this);
    m_message_processor.start();

    m_suicide_timer.set<overseer_t, &overseer_t::timeout>(this);
    m_suicide_timer.start(config_t::get().engine.suicide_timeout);

    m_heartbeat_timer.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0, 5.0);
}

overseer_t::~overseer_t() {
    terminate();
}

void overseer_t::operator()(boost::shared_ptr<source_t> source) {
    m_source = source;
    m_loop.loop();
}

void overseer_t::message(ev::io& w, int revents) {
    if(m_messages.pending() && !m_message_processor.is_active()) {
        m_message_processor.start();
    }
}

void overseer_t::process_message(ev::idle& w, int revents) {
    if(m_messages.pending()) {
        std::string deferred_id;
        unsigned int code = 0;

        boost::tuple<std::string&, unsigned int&> tier(deferred_id, code);
        m_messages.recv_multi(tier);

        switch(code) {
            case INVOKE: {
                std::string method;

                m_messages.recv(method);

                try {
                    if(m_messages.more()) {
                        zmq::message_t request;
                        
                        m_messages.recv(&request);
                       
                        m_source->invoke(
                            boost::bind(&overseer_t::respond, this, deferred_id, _1, _2),
                            method, 
                            request.data(), 
                            request.size());
                    } else {
                        m_source->invoke(
                            boost::bind(&overseer_t::respond, this, deferred_id, _1, _2),
                            method);
                    }
                } catch(const std::exception& e) {
                    syslog(LOG_ERR, "worker [%s]: '%s' invocation failed - %s", 
                        id().c_str(), method.c_str(), e.what());
                    
                    // Report an error 
                    Json::Value object(Json::objectValue);

                    object["error"] = e.what();

                    Json::FastWriter writer;
                    std::string response(writer.write(object));

                    respond(deferred_id, response.data(), response.size());
                }
                    
                boost::this_thread::interruption_point();

                m_messages.send_multi(
                    boost::make_tuple(
                        CHOKE,
                        deferred_id));
                
                // XXX: Damn, ZeroMQ, why are you so strange? 
                m_loop.feed_fd_event(m_messages.fd(), ev::READ);
                
                m_suicide_timer.stop();
                m_suicide_timer.start(config_t::get().engine.suicide_timeout);
                
                break;
            }
            
            case TERMINATE:
                terminate();
                return;
        }
    } else {
        m_message_processor.stop();
    }
}

void overseer_t::respond(const std::string& deferred_id, 
                         const void* response, 
                         size_t size) 
{
    boost::this_thread::interruption_point();
    
    zmq::message_t message(size);
    
    memcpy(message.data(), response, size);
   
    m_messages.send_multi(
        boost::make_tuple(
            CHUNK,
            deferred_id,
            boost::ref(message)));
}

void overseer_t::timeout(ev::timer& w, int revents) {
    m_messages.send(SUICIDE);
    terminate();
}

void overseer_t::heartbeat(ev::timer& w, int revents) {
    m_messages.send(HEARTBEAT);
}

void overseer_t::terminate() {
    m_loop.unloop();
} 

