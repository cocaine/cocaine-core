#include "cocaine/engine.hpp"
#include "cocaine/future.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/threading.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::threading;

using namespace cocaine::core;
using namespace cocaine::plugin;
using namespace cocaine::helpers;

engine_t::engine_t(zmq::context_t& context, const std::string& target):
    m_context(context),
    m_target(target),
    m_default_thread_id(auto_uuid_t().get())
{
    syslog(LOG_INFO, "engine %s: starting", m_target.c_str());
}

engine_t::~engine_t() {
    syslog(LOG_INFO, "engine %s: terminating", m_target.c_str()); 
    m_threads.clear();
}

void engine_t::push(future_t* future, const Json::Value& args) {
    std::string thread_id;

    if(!args.get("isolated", false).asBool()) {
        thread_id = m_default_thread_id;
    } else {
        thread_id = args.get("thread", auto_uuid_t().get()).asString();
    }
    
    thread_map_t::iterator it(m_threads.find(thread_id));

    if(it == m_threads.end()) {
        try {
            std::auto_ptr<thread_t> thread(new thread_t(auto_uuid_t(thread_id), m_context));
            boost::shared_ptr<source_t> source(registry_t::instance()->create(m_target));
            thread->run(source);
            boost::tie(it, boost::tuples::ignore) = m_threads.insert(thread_id, thread);
        } catch(const zmq::error_t& e) {
            if(e.num() == EMFILE) {
                syslog(LOG_DEBUG, "engine %s: too many threads, task queued", m_target.c_str());
                m_pending.push(std::make_pair(future, args));
                return;
            } else {
                throw;
            }
        } catch(const std::exception& e) {
            syslog(LOG_ERR, "engine %s: error - %s", m_target.c_str(), e.what());
            future->abort(m_target, e.what());
            return;
        }
    }
        
    it->second->request(PUSH, future, args);
}

void engine_t::drop(future_t* future, const Json::Value& args) {
    std::string thread_id;

    if(!args.get("isolated", false).asBool()) {
        thread_id = m_default_thread_id;
    } else {
        thread_id = args.get("thread", "").asString();
    }

    thread_map_t::iterator it(m_threads.find(thread_id));

    if(it != m_threads.end()) {
        it->second->request(DROP, future, args);
    } else {
        future->abort(m_target, "thread is not active");
    }
}

void engine_t::track(const std::string& thread_id) {
    
}

void engine_t::reap(const std::string& thread_id) {
    thread_map_t::iterator it(m_threads.find(thread_id));

    if(it == m_threads.end()) {
        syslog(LOG_WARNING, "engine %s: found an orphan - thread %s", 
            m_target.c_str(), thread_id.c_str());
        return;
    }

    m_threads.erase(it);

    // If we got something in the queue, try to invoke it
    if(!m_pending.empty()) {
        future_t* future;
        Json::Value args;

        boost::tie(future, args) = m_pending.front();
        m_pending.pop();
        
        push(future, args);
    }
}
