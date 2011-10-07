#include <boost/bind.hpp>

#include "cocaine/engine.hpp"
#include "cocaine/future.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/threading.hpp"

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
    syslog(LOG_INFO, "engine %s [%s]: starting for", m_id.get().c_str(), m_uri.c_str());
    
    m_link.bind("inproc://engine/" + m_id.get());
    m_link_watcher.set<engine_t, &engine_t::request>(this);
    m_link_watcher.start(m_link.fd(), EV_READ);
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

void engine_t::push(future_t* future, const std::string& target, const Json::Value& args) {
    std::string thread_id(args["thread"].asString());
    thread_map_t::iterator it(m_threads.find(thread_id));

    if(it == m_threads.end()) {
        try {
            std::auto_ptr<thread_t> thread(new thread_t(m_context, m_id, m_uri));
            thread_id = thread->id();
            boost::tie(it, boost::tuples::ignore) = m_threads.insert(thread_id, thread);
        } catch(const zmq::error_t& e) {
            if(e.num() == EMFILE) {
                syslog(LOG_DEBUG, "engine %s [%s]: too many threads, task queued",
                    m_id.get().c_str(), m_uri.c_str());
                m_pending.push(boost::make_tuple(future, target, args));
                return;
            } else {
                throw;
            }
        }
    }
    
    m_link.send_multi(boost::make_tuple(
        protect(thread_id),
        PUSH,
        args));
}

void engine_t::drop(future_t* future, const std::string& target, const Json::Value& args) {
    std::string thread_id(args["thread"].asString());

    if(thread_id.empty()) {
        throw std::runtime_error("no thread id has been specified");
    }

    thread_map_t::iterator it(m_threads.find(thread_id));

    if(it != m_threads.end()) {
        m_link.send_multi(boost::make_tuple(
            protect(thread_id),
            DROP,
            args["key"].asString()));
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

        switch(code) {
            case EVENT: {
                std::string driver_id;
                Json::Value object;

                boost::tuple<std::string&, Json::Value&> tier(driver_id, object);
                m_link.recv_multi(tier);
                
                m_parent->event(driver_id, object);
                
                break;
            }
            case FUTURE: {
                // XXX: TBD
                break;
            }
            case SUICIDE: {
                reap(thread_id);
                break;
            }
            default:
                syslog(LOG_ERR, "engine %s [%s]: [%s()] unknown message",
                    m_id.get().c_str(), m_uri.c_str(), __func__);
        }
    }
}

void engine_t::reap(const std::string& thread_id) {
    thread_map_t::iterator it(m_threads.find(thread_id));

    if(it == m_threads.end()) {
        syslog(LOG_ERR, "engine %s [%s]: [%s()] orphan - thread %s", 
            __func__, m_id.get().c_str(), m_uri.c_str(), thread_id.c_str());
        return;
    }

    m_threads.erase(it);

    // If we got something in the queue, try to invoke it
    if(!m_pending.empty()) {
        // XXX: TBD
    }
}

// Thread interface
// ----------------

thread_t::thread_t(zmq::context_t& context, const auto_uuid_t& engine_id, const std::string& uri):
    m_overseer(new overseer_t(context, engine_id))
{
    syslog(LOG_DEBUG, "thread %s [%s]: starting", m_overseer->id().c_str(), uri.c_str());
    
    try {
        boost::shared_ptr<source_t> source(core::registry_t::instance()->create(uri));
        m_thread.reset(new boost::thread(boost::bind(
            &overseer_t::run, m_overseer.get(), source)));
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("system thread limit reached");
    }
}

thread_t::~thread_t() {
    if(m_thread.get()) {
        syslog(LOG_DEBUG, "thread %s: terminating", m_overseer->id().c_str());
   
#if BOOST_VERSION >= 103500
        using namespace boost::posix_time;
        
        if(!m_thread->timed_join(seconds(config_t::get().engine.linger_timeout))) {
            syslog(LOG_WARNING, "thread %s: thread is unresponsive", m_oversser->id().c_str());
            m_thread->interrupt();
        }
#else
        m_thread->join();
#endif
    }
}

std::string thread_t::id() const {
    return m_overseer->id();
}
