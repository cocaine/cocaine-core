#if BOOST_VERSION < 103500
    #include <boost/bind.hpp>
#endif

#include "cocaine/backends/thread.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"

using namespace cocaine::engine;
using namespace cocaine::plugin;

thread_t::thread_t(boost::shared_ptr<engine_t> parent, boost::shared_ptr<source_t> source):
    m_parent(parent)
{
    syslog(LOG_DEBUG, "worker [%s:%s]: constructing", m_parent->name().c_str(), id().c_str());
   
    try {
        m_overseer.reset(new overseer_t(id(), m_parent->context(), m_parent->name()));

#if BOOST_VERSION >= 103500
        m_thread.reset(new boost::thread(boost::ref(*m_overseer), source));
#else
        m_thread.reset(new boost::thread(boost::bind(
            &overseer_t::run, m_overseer.get(), source)));
#endif
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("system thread limit exceeded");
    }
}

thread_t::~thread_t() {
    syslog(LOG_DEBUG, "worker [%s:%s]: destructing", m_parent->name().c_str(), id().c_str());
    
    if(m_thread) {
#if BOOST_VERSION >= 103500
        if(!m_thread->timed_join(boost::posix_time::seconds(5))) {
            syslog(LOG_WARNING, "worker [%s:%s]: worker is unresponsive",
                m_parent->name().c_str(), id().c_str());
            m_thread->interrupt();
        }
#else
        m_thread->join();
#endif
    }
}

void thread_t::timeout(ev::timer& w, int revents) {
    syslog(LOG_ERR, "worker [%s:%s]: worker missed a heartbeat",
        m_parent->name().c_str(), id().c_str());

#if BOOST_VERSION >= 103500
    m_thread->interrupt();
#endif
    
    // This will detach the thread so it'll die silently when finished
    m_thread.reset();

    // This will signal timeouts to all the clients and destroy the object
    m_parent->reap(id());
}

