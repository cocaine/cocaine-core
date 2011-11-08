#include "cocaine/backends/abstract.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine;

backend_t::backend_t(engine_t* engine):
    m_engine(engine),
    m_settled(false)
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

void backend_t::rearm() {
    m_heartbeat.stop();

    if(m_job) {
        m_heartbeat.start(m_job->timeout());
    } else {
        m_heartbeat.start(config_t::get().engine.heartbeat_timeout);
    }
    
    m_settled = true;
}

void backend_t::assign(boost::shared_ptr<job_t> job) {
    m_job = job;
    rearm();
}

void backend_t::resign() {
    m_job.reset();
    rearm();
}

state_t backend_t::state() const {
    return m_job ? active : (m_settled ? idle : inactive);
}

boost::shared_ptr<job_t> backend_t::job() {
    return m_job;
}

void backend_t::timeout(ev::timer&, int) {
    syslog(LOG_ERR, "worker [%s:%s]: worker has missed too many heartbeats",
        m_engine->name().c_str(), id().c_str());
    
    kill();

    if(m_job) {
        m_job->abort(timeout_error, "the request has timed out");
        m_job.reset();
    }

    m_engine->reap(id());
}
