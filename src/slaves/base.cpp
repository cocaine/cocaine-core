#include "cocaine/drivers/base.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/slaves/base.hpp"

using namespace cocaine::engine::slave;

slave_t::slave_t(engine_t* engine):
    m_engine(engine)
{
    syslog(LOG_DEBUG, "slave [%s:%s]: constructing", m_engine->name().c_str(), id().c_str());

    // NOTE: These are the 10 seconds for the slave to come alive   
    m_heartbeat_timer.set<slave_t, &slave_t::timeout>(this);
    m_heartbeat_timer.start(10.0f);

    initiate();
}

slave_t::~slave_t() {
    syslog(LOG_DEBUG, "slave [%s:%s]: destructing", m_engine->name().c_str(), id().c_str());
    
    m_heartbeat_timer.stop();
    
    // TEST: Make sure that the slave is really dead
    BOOST_ASSERT(state_downcast<const dead*>() != 0);

    terminate();
}

void slave_t::react(const events::heartbeat_t& event) {
    m_heartbeat_timer.stop();
    
    const busy* state = state_downcast<const busy*>();

    if(state && state->job()->policy().timeout) {
        m_heartbeat_timer.start(state->job()->policy().timeout);
    } else {
        m_heartbeat_timer.start(config_t::get().engine.heartbeat_timeout);
    }
}

void slave_t::timeout(ev::timer&, int) {
    syslog(LOG_ERR, "slave [%s:%s]: slave has missed too many heartbeats",
        m_engine->name().c_str(), id().c_str());
    
    const busy* state = state_downcast<const busy*>();
    
    if(state) {
        state->job()->process_event(events::timeout_t("the job has timed out"));
    }
    
    process_event(events::terminated_t());
}

void alive::react(const events::invoked_t& event) {
    m_job = event.job;
    m_job->process_event(event);
}

void alive::react(const events::choked_t& event) {
    m_job->process_event(event);
    m_job.reset();
}

alive::~alive() {
    if(m_job && !m_job->state_downcast<const job::complete*>()) {
        syslog(LOG_INFO, "engine [%s]: rescheduling an incomplete '%s' job",
            m_job->driver()->engine()->name().c_str(), m_job->driver()->method().c_str());
        m_job->driver()->engine()->enqueue(m_job, true);
    }
}

dead::dead(my_context ctx):
    my_base(ctx)
{
    context<slave_t>().reap();
}

