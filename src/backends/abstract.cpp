#include "cocaine/backends/abstract.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine;

backend_t::backend_t(engine_t* engine):
    m_engine(engine),
    m_settled(false)
{
    syslog(LOG_DEBUG, "worker [%s:%s]: constructing", m_engine->name().c_str(), id().c_str());

    // NOTE: These are 10 seconds for the worker to come alive   
    m_heartbeat_timer.set<backend_t, &backend_t::timeout>(this);
    m_heartbeat_timer.start(10.);
}

backend_t::~backend_t() {
    syslog(LOG_DEBUG, "worker [%s:%s]: destructing", m_engine->name().c_str(), id().c_str());
    
    if(m_heartbeat_timer.is_active()) {
        m_heartbeat_timer.stop();
    }
}

void backend_t::rearm(float timeout) {
    m_heartbeat_timer.stop();
    m_heartbeat_timer.start(timeout);
    
    m_settled = true;
}

backend_state_t backend_t::state() const {
    return m_job ? active : (m_settled ? idle : inactive);
}

boost::shared_ptr<job_t>& backend_t::job() {
    return m_job;
}

void backend_t::timeout(ev::timer&, int) {
    syslog(LOG_ERR, "worker [%s:%s]: worker has missed too many heartbeats",
        m_engine->name().c_str(), id().c_str());
    
    kill();

    if(m_job) {
        m_job->send(timeout_error, "the request has timed out");
        m_job.reset();
    }

    m_engine->reap(id());
}
