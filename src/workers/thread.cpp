#include <boost/bind.hpp>

#include "cocaine/engine.hpp"
#include "cocaine/future.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/workers/thread.hpp"

using namespace cocaine::engine;
using namespace cocaine::helpers;
using namespace cocaine::lines;
using namespace cocaine::plugin;

thread_t::thread_t(boost::shared_ptr<engine_t> parent, boost::shared_ptr<overseer_t> overseer):
    unique_id_t(overseer->id()),
    m_parent(parent),
    m_overseer(overseer)
{
    syslog(LOG_DEBUG, "thread %s [%s]: constructing", id().c_str(), m_parent->id().c_str());
   
    try {
#if BOOST_VERSION >= 103500
        m_thread.reset(new boost::thread(boost::bind(&overseer_t::run, m_overseer.get())));
#else
        m_thread.reset(new boost::thread());
#endif
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("system thread limit exceeded");
    }

    // First heartbeat is only to ensure that the thread has started
    m_heartbeat.set<thread_t, &thread_t::timeout>(this);
    rearm(10.);
}

thread_t::~thread_t() {
    syslog(LOG_DEBUG, "thread %s [%s]: destructing", id().c_str(), m_parent->id().c_str());
    
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }

    if(m_thread) {
        syslog(LOG_DEBUG, "thread %s [%s]: trying to join", id().c_str(), m_parent->id().c_str());

#if BOOST_VERSION >= 103500
        if(!m_thread->timed_join(boost::posix_time::seconds(5))) {
            syslog(LOG_WARNING, "thread %s [%s]: thread is unresponsive",
                id().c_str(), m_parent->id().c_str());
            m_thread->interrupt();
        }
#else
        m_thread->join();
#endif
    }
}

void thread_t::timeout(ev::timer& w, int revents) {
    syslog(LOG_ERR, "thread %s [%s]: thread missed a heartbeat", id().c_str(), m_parent->id().c_str());

#if BOOST_VERSION >= 103500
    m_thread->interrupt();
#endif
    
    m_thread.reset();

    // Send error to the client of the timed out thread
    Json::Value object(Json::objectValue);

    object["error"] = "timed out";
    m_queue.front()->push(object);
    m_queue.pop();

    // The parent will requeue leftover the tasks to the other threads
    m_parent->reap(id());
}

void thread_t::rearm(float timeout) {
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }

    m_heartbeat.start(timeout);
}

void thread_t::queue_push(boost::shared_ptr<future_t> future) {
    m_queue.push(future);
}

boost::shared_ptr<future_t> thread_t::queue_pop() {
    boost::shared_ptr<future_t> future = m_queue.front();
    m_queue.pop();

    return future;
}

size_t thread_t::queue_size() const {
    return m_queue.size();
}
