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
    m_heartbeat_timer.start(10.0f);
}

backend_t::~backend_t() {
    syslog(LOG_DEBUG, "worker [%s:%s]: destructing", m_engine->name().c_str(), id().c_str());
    m_heartbeat_timer.stop();
}

void backend_t::rearm() {
    m_heartbeat_timer.stop();

    if(m_job && m_job->policy().timeout) {
        m_heartbeat_timer.start(m_job->policy().timeout);
    } else {
        m_heartbeat_timer.start(config_t::get().engine.heartbeat_timeout);
    }
    
    m_settled = true;
}

backend_state backend_t::state() const {
    return m_settled ? (m_job ? active : idle) : inactive;
}

void backend_t::timeout(ev::timer&, int) {
    syslog(LOG_ERR, "worker [%s:%s]: worker has missed too many heartbeats",
        m_engine->name().c_str(), id().c_str());
    
    kill();

    if(m_job) {
        m_job->send(timeout_error, "the job has timed out");
        m_job.reset();
    }

    m_engine->reap(id());
}
