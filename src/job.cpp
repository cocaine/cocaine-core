#include "cocaine/drivers/abstract.hpp"
#include "cocaine/job.hpp"

using namespace cocaine::engine;

job_policy::job_policy():
    urgent(false),
    timeout(config_t::get().engine.heartbeat_timeout),
    deadline(0.0)
{ }

job_policy::job_policy(bool urgent_, ev::tstamp timeout_, ev::tstamp deadline_):
    urgent(urgent_),
    timeout(timeout_),
    deadline(deadline_)
{ }

job_t::job_t(driver_t* parent):
    m_parent(parent)
{ }

job_state job_t::enqueue() {
    return m_parent->engine()->enqueue(
        shared_from_this(),
        boost::make_tuple(
            INVOKE,
            m_parent->method()));
}

job_state job_t::enqueue(job_policy policy) {
    m_policy = policy;
    
    job_state state = enqueue();

    if(state == queued && m_policy.deadline) {
        m_expiration_timer.set<job_t, &job_t::expire>(this);
        m_expiration_timer.start(m_policy.deadline);
    }

    return state;
}

void job_t::audit(ev::tstamp spent) {
    m_parent->audit(spent);
}

void job_t::expire(ev::periodic&, int) {
    m_parent->expire(shared_from_this());
}

