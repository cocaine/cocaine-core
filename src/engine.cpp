#include <boost/bind.hpp>

#include "cocaine/core.hpp"
#include "cocaine/drivers/abstract.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/overseer.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::helpers;
using namespace cocaine::lines;
using namespace cocaine::plugin;

engine_t::engine_t(zmq::context_t& context, boost::shared_ptr<core_t> parent, std::string uri):
    m_context(context),
    m_channel(m_context, ZMQ_ROUTER),
    m_parent(parent),
    m_uri(uri)
{
    syslog(LOG_INFO, "engine %s [%s]: starting", id().c_str(), m_uri.c_str());
    
    m_channel.bind("inproc://engine/" + id());
    m_watcher.set<engine_t, &engine_t::request>(this);
    m_watcher.start(m_channel.fd(), EV_READ);

    // Have to bootstrap it
    ev::get_default_loop().feed_fd_event(m_channel.fd(), EV_READ);
}

engine_t::~engine_t() {
    syslog(LOG_INFO, "engine %s [%s]: terminating", id().c_str(), m_uri.c_str()); 
    
    m_watcher.stop();
    
    for(thread_map_t::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
        m_channel.send_multi(boost::make_tuple(
            protect(it->first),
            TERMINATE));
        m_threads.erase(it);
    }
}

// Operations
// ----------

void engine_t::request(ev::io& w, int revents) {
    std::string thread_id;
    unsigned int code = 0;
   
    while(m_channel.pending()) {
        boost::tuple<raw<std::string>, unsigned int&> tier(protect(thread_id), code);
        m_channel.recv_multi(tier);

        thread_map_t::iterator thread(m_threads.find(thread_id));

        if(thread == m_threads.end()) {
            syslog(LOG_ERR, "engine %s [%s]: [%s()] outstanding messages - thread %s", 
                id().c_str(), m_uri.c_str(), __func__, thread_id.c_str());
            m_channel.ignore();
        }
        
        switch(code) {
            case FUTURE: {
                boost::shared_ptr<future_t> future(thread->second->queue_pop());
                Json::Value object;
                        
                m_channel.recv(object);
                object["thread"] = thread_id;

                future->push(object);

                break;
            }

            case EVENT: {
                std::string driver_id;
                Json::Value object;

                boost::tuple<std::string&, Json::Value&> tier(driver_id, object);
                m_channel.recv_multi(tier);

                // XXX: Per-engine sockets, drop parent dependency?
                m_parent->event(driver_id, object);
                
                break;
            }

            case HEARTBEAT: {
                thread->second->rearm();
                break;
            }

            case SUICIDE: {
                m_channel.send_multi(boost::make_tuple(
                    lines::protect(thread->second->id()),
                    TERMINATE));
                m_threads.erase(thread);
                break;
            }

#ifndef NDEBUG      
            default:
                syslog(LOG_ERR, "engine %s [%s]: [%s()] unknown message",
                    id().c_str(), m_uri.c_str(), __func__);
                abort();
        }
#endif
    }
}

// Thread interface
// ----------------

thread_t::thread_t(unique_id_t::type id_, unique_id_t::type engine_id, zmq::context_t& context, std::string uri):
    unique_id_t(id_),
    m_engine_id(engine_id),
    m_context(context),
    m_uri(uri)
{
    syslog(LOG_DEBUG, "thread %s [%s]: starting", id().c_str(), m_engine_id.c_str());
   
    // Creates an overseer and attaches it to a thread 
    create();

    // Starts the heartbeat timeout
    m_heartbeat.set<thread_t, &thread_t::timeout>(this);
    rearm();
}

thread_t::~thread_t() {
    if(m_thread) {
        syslog(LOG_DEBUG, "thread %s [%s]: terminating", id().c_str(), m_engine_id.c_str());

        m_heartbeat.stop();

#if BOOST_VERSION >= 103500
        if(!m_thread->timed_join(boost::posix_time::seconds(5))) {
            syslog(LOG_WARNING, "thread %s [%s]: thread is unresponsive",
                id().c_str(), m_engine_id.c_str());
            m_thread->interrupt();
        }
#else
        m_thread->join();
#endif
    }
}

void thread_t::create() {
    try {
        // Init the overseer and all the sockets
        m_overseer.reset(new overseer_t(id(), m_engine_id, m_context));

        // Create a new source
        boost::shared_ptr<source_t> source(core::registry_t::instance()->create(m_uri));
        
        // Launch the thread
        m_thread.reset(new boost::thread(boost::bind(
            &overseer_t::run, m_overseer.get(), source)));
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("system thread limit reached");
    }
}

void thread_t::timeout(ev::timer& w, int revents) {
    syslog(LOG_ERR, "thread %s [%s]: thread missed a heartbeat", id().c_str(), m_engine_id.c_str());

#if BOOST_VERSION >= 103500
    m_thread->interrupt();
#endif

    create();
    rearm();
}

void thread_t::rearm() {
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }

    m_heartbeat.start(config_t::get().engine.heartbeat_timeout);
}
