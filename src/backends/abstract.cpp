#include "cocaine/backends/abstract.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine;

backend_t::backend_t(engine_t* engine):
    m_engine(engine),
    m_active(false)
{
    syslog(LOG_DEBUG, "worker [%s:%s]: constructing", m_engine->name().c_str(), id().c_str());

    // NOTE: These are 10 seconds for the worker to come alive   
    m_heartbeat.set<backend_t, &backend_t::timeout>(this);
    m_heartbeat.start(10.);
}

backend_t::~backend_t() {
    syslog(LOG_DEBUG, "worker [%s:%s]: destructing", m_engine->name().c_str(), id().c_str());
    
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }
}

bool backend_t::active() const {
    return m_active;
}

void backend_t::rearm(float timeout) {
    m_heartbeat.stop();
    m_heartbeat.start(timeout);

    m_active = true;
}

backend_t::job_queue_t& backend_t::queue() {
    return m_queue;
}

const backend_t::job_queue_t& backend_t::queue() const {
    return m_queue;
}

void backend_t::timeout(ev::timer&, int) {
    syslog(LOG_ERR, "worker [%s:%s]: worker has missed too many heartbeats",
        m_engine->name().c_str(), id().c_str());
    
    kill();

    if(m_queue.size()) {
        m_queue.front()->abort(timeout_error, "the request has timed out");
        m_queue.pop();
    }

    m_engine->reap(id());
}
