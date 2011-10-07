#include <boost/bind.hpp>

#include "cocaine/core.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/future.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/overseer.hpp"

#include "cocaine/drivers/abstract.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::helpers;
using namespace cocaine::lines;
using namespace cocaine::plugin;

engine_t::engine_t(zmq::context_t& context, boost::shared_ptr<core_t> parent, const std::string& uri):
    m_context(context),
    m_link(m_context, ZMQ_ROUTER),
    m_parent(parent),
    m_uri(uri)
{
    syslog(LOG_INFO, "engine %s [%s]: starting", m_id.get().c_str(), m_uri.c_str());
    
    m_link.bind("inproc://engine/" + m_id.get());
    m_link_watcher.set<engine_t, &engine_t::request>(this);
    m_link_watcher.start(m_link.fd(), EV_READ);

    // Have to bootstrap it
    ev::get_default_loop().feed_fd_event(m_link.fd(), EV_READ);
}

engine_t::~engine_t() {
    syslog(LOG_INFO, "engine %s [%s]: terminating", m_id.get().c_str(), m_uri.c_str()); 
    
    m_link_watcher.stop();
    
    for(thread_map_t::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
        m_link.send_multi(boost::make_tuple(
            protect(it->first),
            TERMINATE));
    }

    m_threads.clear();
}

boost::shared_ptr<future_t> engine_t::push(const Json::Value& args) {
    boost::shared_ptr<future_t> future(new future_t());
    std::string thread_id(args["thread"].asString());
    thread_map_t::iterator thread(m_threads.find(thread_id));

    if(thread == m_threads.end()) {
        try {
            std::auto_ptr<thread_t> object(new thread_t(m_context, m_id, m_uri));
            thread_id = object->id();
            boost::tie(thread, boost::tuples::ignore) = m_threads.insert(thread_id, object);
        } catch(const zmq::error_t& e) {
            if(e.num() == EMFILE) {
                syslog(LOG_DEBUG, "engine %s [%s]: too many threads, task queued",
                    m_id.get().c_str(), m_uri.c_str());
                m_pending.push(std::make_pair(future, args));
                return future;
            } else {
                throw;
            }
        }
    }
    
    thread->second->enqueue(future);

    m_link.send_multi(boost::make_tuple(
        protect(thread_id),
        PUSH,
        args));

    return future;
}

boost::shared_ptr<future_t> engine_t::drop(const Json::Value& args) {
    boost::shared_ptr<future_t> future(new future_t());
    std::string thread_id(args["thread"].asString());

    if(thread_id.empty()) {
        throw std::runtime_error("no thread id has been specified");
    }

    thread_map_t::iterator thread(m_threads.find(thread_id));

    if(thread != m_threads.end()) {
        thread->second->enqueue(future);

        m_link.send_multi(boost::make_tuple(
            protect(thread_id),
            DROP,
            args["key"].asString()));

        return future;
    } else {
        throw std::runtime_error("thread is not active");
    }
}

void engine_t::request(ev::io& w, int revents) {
    std::string thread_id;
    unsigned int code = 0;
   
    while(m_link.pending()) {
        boost::tuple<raw<std::string>, unsigned int&> tier(protect(thread_id), code);
        m_link.recv_multi(tier);

        thread_map_t::iterator thread(m_threads.find(thread_id));
        
        if(thread == m_threads.end()) {
            syslog(LOG_ERR, "engine %s [%s]: [%s()] orphan - thread %s", 
                __func__, m_id.get().c_str(), m_uri.c_str(), thread_id.c_str());
            abort();
        }
        
        switch(code) {
            case FUTURE: {
                boost::shared_ptr<future_t> future(thread->second->dequeue());
                Json::Value object;
                        
                m_link.recv(object);
                object["thread"] = thread_id;

                future->push(object);

                break;
            }
            case EVENT: {
                std::string driver_id;
                Json::Value object;

                boost::tuple<std::string&, Json::Value&> tier(driver_id, object);
                m_link.recv_multi(tier);

                // TODO: Per-engine sockets, drop parent dependency?
                m_parent->event(driver_id, object);
                
                break;
            }
#if BOOST_VERSION >= 103500
            case HEARTBEAT: {
                thread->second->rearm();
                break;
            }
#endif
            case SUICIDE: {
                m_threads.erase(thread);

                // If we got something in the queue, try to invoke it
                if(!m_pending.empty()) {
                    boost::shared_ptr<future_t> future;
                    Json::Value args;

                    boost::tie(future, args) = m_pending.front();
                    m_pending.pop();

                    push(args)->move(future);
                }
                
                break;
            }
            default:
                syslog(LOG_ERR, "engine %s [%s]: [%s()] unknown message",
                    m_id.get().c_str(), m_uri.c_str(), __func__);
                abort();
        }
    }
}

// Thread interface
// ----------------

thread_t::thread_t(zmq::context_t& context, const auto_uuid_t& engine_id, const std::string& uri):
    m_context(context),
    m_engine_id(engine_id),
    m_uri(uri)
{
    syslog(LOG_DEBUG, "thread %s [%s]: starting", m_id.get().c_str(), uri.c_str());
    
    create();

#if BOOST_VERSION >= 103500
    m_heartbeat.set<thread_t, &thread_t::timeout>(this);
    rearm();
#endif    
}

thread_t::~thread_t() {
    if(m_thread.get()) {
        syslog(LOG_DEBUG, "thread %s: terminating", m_overseer->id().c_str());
   
#if BOOST_VERSION >= 103500
        using namespace boost::posix_time;
        
        if(!m_thread->timed_join(seconds(config_t::get().engine.linger_timeout))) {
            syslog(LOG_WARNING, "thread %s: thread is unresponsive", m_overseer->id().c_str());
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
        m_overseer.reset(new overseer_t(m_context, m_engine_id, m_id));

        // Create a new source
        boost::shared_ptr<source_t> source(core::registry_t::instance()->create(m_uri));
        
        // Launch the thread
        m_thread.reset(new boost::thread(boost::bind(
            &overseer_t::run, m_overseer.get(), source)));
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("system thread limit reached");
    }
}

#if BOOST_VERSION >= 103500
void thread_t::timeout(ev::timer& w, int revents) {
    syslog(LOG_ERR, "thread %s: thread is unresponsive, restarting",
        m_id.get().c_str());
    
    m_thread->interrupt();
    m_thread.reset();

    create();
    rearm();
}
#endif

std::string thread_t::id() const {
    return m_id.get();
}

void thread_t::enqueue(boost::shared_ptr<future_t> future) {
    m_queue.push(future);
}

boost::shared_ptr<future_t> thread_t::dequeue() {
    boost::shared_ptr<future_t> future(m_queue.front());
    m_queue.pop();

    return future;
}

#if BOOST_VERSION >= 103500
void thread_t::rearm() {
    m_heartbeat.stop();
    m_heartbeat.start(60.0);
}
#endif
