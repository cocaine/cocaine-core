//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
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
#include "cocaine/engine.hpp"
#include "cocaine/job.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine::engine::slaves;

slave_t::slave_t(engine_t& engine):
    object_t(engine.context()),
    m_engine(engine)
{
    // NOTE: These are the 10 seconds for the slave to come alive.
    m_heartbeat_timer.set<slave_t, &slave_t::timeout>(this);
    m_heartbeat_timer.start(10.0f);

    initiate();
}

slave_t::~slave_t() {
    m_heartbeat_timer.stop();
    
    // TEST: Make sure that the slave is really dead.
    BOOST_ASSERT(state_downcast<const dead*>() != 0);

    terminate();
}

void slave_t::react(const events::heartbeat_t& event) {
#if EV_VERSION_MAJOR == 3 && EV_VERSION_MINOR == 8
    if(!state_downcast<const alive*>()) {
        m_engine.app().log->debug(
            "slave %s came alive in %.03f seconds",
            id().c_str(),
            10.0f - ev_timer_remaining(
                ev_default_loop(ev::AUTO),
                static_cast<ev_timer*>(&m_heartbeat_timer)
            )
        );
    }
#endif

    m_heartbeat_timer.stop();
    
    const busy* state = state_downcast<const busy*>();
    float timeout = object_t::context().config.engine.heartbeat_timeout;

    if(state && state->job()->policy().timeout > 0.0f) {
        timeout = state->job()->policy().timeout;
    }
    
    m_engine.app().log->debug(
        "resetting the heartbeat timeout for slave %s to %.02f seconds", 
        id().c_str(),
        timeout
    );
        
    m_heartbeat_timer.start(timeout);

}

void slave_t::react(const events::terminate_t& event) {
    m_engine.app().log->debug(
        "reaping slave %s", 
        id().c_str()
    );

    reap();
}

bool slave_t::operator==(const slave_t& other) const {
    return id() == other.id();
}

void slave_t::timeout(ev::timer&, int) {
    m_engine.app().log->warning(
        "slave %s missed too many heartbeats",
        id().c_str()
    );
    
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
    if(m_job && !m_job->state_downcast<const complete*>()) {
        context<slave_t>().m_engine.app().log->debug(
            "rescheduling an incomplete '%s' job", 
            m_job->method().c_str()
        );
        
        context<slave_t>().m_engine.enqueue(m_job, true);
        m_job.reset();
    }
}

void alive::react(const events::invoke_t& event) {
    // TEST: Ensure that no job is being lost here.
    BOOST_ASSERT(!m_job);

    m_job = event.job;
    m_job->process_event(event);
    
    context<slave_t>().m_engine.app().log->debug(
        "slave %s got a '%s' job",
        context<slave_t>().id().c_str(),
        m_job->method().c_str()
    );
}

void alive::react(const events::release_t& event) {
    // TEST: Ensure that the job is in fact here.
    BOOST_ASSERT(m_job);

    context<slave_t>().m_engine.app().log->debug(
        "slave %s completed a '%s' job",
        context<slave_t>().id().c_str(),
        m_job->method().c_str()
    );
    
    m_job->process_event(event);
    m_job.reset();
}
