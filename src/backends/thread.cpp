#include "cocaine/backends/thread.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::backends;

thread_t::thread_t(boost::shared_ptr<engine_t> engine, const std::string& type, const std::string& args):
    m_engine(engine)
{
    syslog(LOG_DEBUG, "worker [%s:%s]: constructing", m_engine->name().c_str(), id().c_str());
   
    try {
        m_overseer.reset(new overseer_t(id(), m_engine->context(), m_engine->name()));
        m_thread.reset(new boost::thread(boost::ref(*m_overseer), type, args));
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("unable to spawn more workers");
    }
}

thread_t::~thread_t() {
    syslog(LOG_DEBUG, "worker [%s:%s]: destructing", m_engine->name().c_str(), id().c_str());
    
    if(m_thread) {
        if(!m_thread->timed_join(boost::posix_time::seconds(5))) {
            syslog(LOG_WARNING, "worker [%s:%s]: worker is unresponsive",
                m_engine->name().c_str(), id().c_str());
            m_thread->interrupt();
        }
    }
}

void thread_t::timeout(ev::timer& w, int revents) {
    syslog(LOG_ERR, "worker [%s:%s]: worker missed a heartbeat",
        m_engine->name().c_str(), id().c_str());

    m_thread->interrupt();
    
    // This will detach the thread so it'll die silently when finished
    m_thread.reset();

    // This will signal timeouts to all the clients and destroy the object
    m_engine->reap(id());
}

