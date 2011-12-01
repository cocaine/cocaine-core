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
    
    // TEST: Make sure that the slave is really dead
    BOOST_ASSERT(state_downcast<const dead*>() != 0);

    m_heartbeat_timer.stop();
    
    terminate();
}

void slave_t::react(const events::heartbeat& event) {
    m_heartbeat_timer.stop();
    
    const busy* state = state_downcast<const busy*>();

    if(state) {
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
        state->job()->process_event(events::timeout_error("the job has timed out"));
    }
    
    process_event(events::death());
}

void alive::react(const events::invoked& event) {
    m_job = event.job;
    m_job->process_event(event);
}

void alive::react(const events::completed& event) {
    m_job->process_event(event);
    m_job.reset();
}

alive::~alive() {
    BOOST_ASSERT(!m_job);
}

dead::dead(my_context ctx):
    my_base(ctx)
{
    context<slave_t>().reap();
}

