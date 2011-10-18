#include <boost/bind.hpp>

#include "cocaine/future.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/workers/thread.hpp"

using namespace cocaine::engine;
using namespace cocaine::helpers;
using namespace cocaine::lines;
using namespace cocaine::plugin;

thread_t::thread_t(unique_id_t::type engine_id, zmq::context_t& context, const std::string& uri):
    m_engine_id(engine_id),
    m_context(context),
    m_uri(uri)
{
    syslog(LOG_DEBUG, "thread %s [%s]: starting", id().c_str(), m_engine_id.c_str());
   
    m_heartbeat.set<thread_t, &thread_t::timeout>(this);
    
    // Creates an overseer and attaches it to a thread 
    create();
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

        // First heartbeat is only to ensure that the thread has started
        rearm(10.);
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("system thread limit exceeded");
    }
}

void thread_t::timeout(ev::timer& w, int revents) {
    syslog(LOG_ERR, "thread %s [%s]: thread missed a heartbeat", id().c_str(), m_engine_id.c_str());

#if BOOST_VERSION >= 103500
    m_thread->interrupt();
#endif

    // Send timeouts to all the pending requests
    Json::Value object(Json::objectValue);
    object["error"] = "timed out";

    while(!m_queue.empty()) {
        m_queue.front()->push(object);
        m_queue.pop();
    }

    // XXX: This doesn't work yet, as ZeroMQ doesn't support two sockets with the same identity
    // XXX: i.e., when the old thread has been interrupted, and a new one spawns with the same id
    // XXX: the new socket should overtake the older identity, but it crashes instead
    create();
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
