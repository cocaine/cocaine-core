#include "cocaine/overseer.hpp"
#include "cocaine/plugin.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::drivers;
using namespace cocaine::plugin;
using namespace cocaine::helpers;

overseer_t::overseer_t(zmq::context_t& context, unique_id_t::type engine_id, boost::shared_ptr<source_t> source):
    m_context(context),
    m_messages(m_context, ZMQ_DEALER, id()),
    m_loop(),
    m_message_watcher(m_loop),
    m_message_processor(m_loop),
    m_suicide_timer(m_loop),
    m_heartbeat_timer(m_loop),
    m_source(source)
{
    syslog(LOG_DEBUG, "overseer %s: constructing", id().c_str());

    // Connect to the engine's controlling socket and set the socket watcher
    m_messages.connect("inproc://engines/" + engine_id);

    m_message_watcher.set<overseer_t, &overseer_t::message>(this);
    m_message_watcher.start(m_messages.fd(), EV_READ);
    m_message_processor.set<overseer_t, &overseer_t::process_message>(this);
    m_message_processor.start();

    // Initializing suicide timer
    m_suicide_timer.set<overseer_t, &overseer_t::timeout>(this);
    m_suicide_timer.start(config_t::get().engine.suicide_timeout);

    // Initialize heartbeat timer
    m_heartbeat_timer.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat_timer.start(5.0, 5.0);
}

overseer_t::~overseer_t() {
    syslog(LOG_DEBUG, "overseer %s: destructing", id().c_str());
}

void overseer_t::run() {
    m_loop.loop();
}

void overseer_t::message(ev::io& w, int revents) {
    if(m_messages.pending() && !m_message_processor.is_active()) {
        m_message_processor.start();
    }
}

void overseer_t::process_message(ev::idle& w, int revents) {
    if(m_messages.pending()) {
        Json::Value result;
        
        unsigned int code = 0;
        m_messages.recv(code);

        switch(code) {
            case INVOKE: {
                std::string task;

                m_messages.recv(task);

                try {
                    if(!m_messages.has_more()) {
                        result = m_source->invoke(task);
                    } else {
                        std::string blob;
                        m_messages.recv(blob);
                        result = m_source->invoke(task, blob.data(), blob.size());
                    }
                } catch(const std::exception& e) {
                    syslog(LOG_ERR, "overseer %s: [%s()] %s", id().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                m_suicide_timer.stop();
                m_suicide_timer.start(config_t::get().engine.suicide_timeout);

                m_messages.send_multi(boost::make_tuple(
                    FUTURE,
                    result)); 
                
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

