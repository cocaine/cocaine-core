//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "cocaine/slaves/base.hpp"

#include "cocaine/context.hpp"
#include "cocaine/drivers/base.hpp"
#include "cocaine/engine.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine::engine::slave;

slave_t::slave_t(context_t& context):
    m_context(context),
    m_log(context, "slave " + id())
{
    m_log.debug("constructing");

    // NOTE: These are the 10 seconds for the slave to come alive   
    m_heartbeat_timer.set<slave_t, &slave_t::timeout>(this);
    m_heartbeat_timer.start(10.0f);

    initiate();
}

slave_t::~slave_t() {
    m_log.debug("destructing");
    
    m_heartbeat_timer.stop();
    
    // TEST: Make sure that the slave is really dead
    BOOST_ASSERT(state_downcast<const dead*>() != 0);

    terminate();
}

void slave_t::react(const events::heartbeat_t& event) {
#if EV_VERSION_MAJOR == 3 && EV_VERSION_MINOR == 8
    if(!state_downcast<const alive*>()) {
        m_log.debug("came alive in %.03f seconds",
            10.0f - ev_timer_remaining(
                ev_default_loop(ev::AUTO),
                static_cast<ev_timer*>(&m_heartbeat_timer)
            )
        );
    }
#endif

    m_heartbeat_timer.stop();
    
    const busy* state = state_downcast<const busy*>();
    float timeout = m_context.config.engine.heartbeat_timeout;

    if(state && state->job()->policy().timeout > 0.0f) {
        timeout = state->job()->policy().timeout;
    }
    
    m_log.debug(
        "resetting the heartbeat timeout to %.02f seconds", 
        m_context.config.engine.heartbeat_timeout
    );
        
    m_heartbeat_timer.start(timeout);

}

void slave_t::react(const events::terminate_t& event) {
    m_log.debug("reaping");
    reap();
}

bool slave_t::operator==(const slave_t& other) const {
    return id() == other.id();
}

void slave_t::timeout(ev::timer&, int) {
    m_log.warning("missed too many heartbeats");
    
    const busy* state = state_downcast<const busy*>();
    
    if(state) {
        state->job()->process_event(
            events::error_t(
                client::timeout_error, 
                "the job has timed out"
            )
        );
    }
    
    process_event(events::terminate_t());
}

alive::~alive() {
    if(m_job && !m_job->state_downcast<const job::complete*>()) {
        context<slave_t>().log().debug(
            "rescheduling an incomplete '%s' job",
            m_job->driver().method().c_str()
        );
       
        // NOTE: Allow the queue to grow beyond its capacity. 
        m_job->driver().engine().enqueue(m_job, true);
        m_job.reset();
    }
}

void alive::react(const events::invoke_t& event) {
    // TEST: Ensure that no job is being lost here
    BOOST_ASSERT(!m_job);

    context<slave_t>().log().debug(
        "assigned a '%s' job",
        event.job->driver().method().c_str()
    );
    
    m_job = event.job;
    m_job->process_event(event);
}

void alive::react(const events::release_t& event) {
    // TEST: Ensure that the job is in fact here
    BOOST_ASSERT(m_job);

    context<slave_t>().log().debug(
        "completed a '%s' job",
        m_job->driver().method().c_str()
    );
    
    m_job->process_event(event);
    m_job.reset();
}
