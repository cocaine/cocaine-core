#include "cocaine/engine.hpp"
#include "cocaine/future.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/threading.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::threading;

using namespace cocaine::core;
using namespace cocaine::plugin;
using namespace cocaine::helpers;

engine_t::engine_t(zmq::context_t& context, const std::string& uri):
    m_context(context),
    m_uri(uri)
{
    syslog(LOG_INFO, "engine %s: starting", m_uri.c_str());
}

engine_t::~engine_t() {
    syslog(LOG_INFO, "engine %s: terminating", m_uri.c_str()); 
    m_threads.clear();
}

void engine_t::push(future_t* future, const std::string& target, const Json::Value& args) {
    std::string thread_id(args.get("thread", auto_uuid_t().get()).asString());
    thread_map_t::iterator it(m_threads.find(thread_id));

    if(it == m_threads.end()) {
        try {
            std::auto_ptr<thread_t> thread(new thread_t(auto_uuid_t(thread_id), m_context));
            boost::shared_ptr<source_t> source(registry_t::instance()->create(m_uri));
            thread->run(source);
            boost::tie(it, boost::tuples::ignore) = m_threads.insert(thread_id, thread);
        } catch(const zmq::error_t& e) {
            if(e.num() == EMFILE) {
                syslog(LOG_DEBUG, "engine %s: too many threads, task queued", m_uri.c_str());
                m_pending.push(boost::make_tuple(future, target, args));
                return;
            } else {
                throw;
            }
        }
    }
        
    it->second->push(future, target, args);
}

void engine_t::drop(future_t* future, const std::string& target, const Json::Value& args) {
    std::string thread_id(args.get("thread", "").asString());

    if(thread_id.empty()) {
        throw std::runtime_error("no thread id has been specified");
    }

    thread_map_t::iterator it(m_threads.find(thread_id));

    if(it != m_threads.end()) {
        it->second->drop(future, target, args);
    } else {
        throw std::runtime_error("thread is not active");
    }
}

void engine_t::track(const std::string& thread_id) {
    
}

void engine_t::reap(const std::string& thread_id) {
    thread_map_t::iterator it(m_threads.find(thread_id));

    if(it == m_threads.end()) {
        syslog(LOG_ERR, "engine %s: [%s()] orphan - thread %s", 
            __func__, m_uri.c_str(), thread_id.c_str());
        return;
    }

    m_threads.erase(it);

    // If we got something in the queue, try to invoke it
    if(!m_pending.empty()) {
        future_t* future;
        std::string target;
        Json::Value args;

        boost::tie(future, target, args) = m_pending.front();
        m_pending.pop();
        
        push(future, target, args);
    }
}
