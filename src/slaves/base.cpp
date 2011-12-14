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

#include <boost/format.hpp>

#include "cocaine/client/types.hpp"
#include "cocaine/drivers/base.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/slaves/base.hpp"

using namespace cocaine::engine::slave;

slave_t::slave_t(engine_t* engine):
    identifiable_t((boost::format("slave [%1%:%2%]") % engine->name() % id()).str()),
    m_engine(engine)
{
    syslog(LOG_DEBUG, "%s: constructing", identity());

    // NOTE: These are the 10 seconds for the slave to come alive   
    m_heartbeat_timer.set<slave_t, &slave_t::timeout>(this);
    m_heartbeat_timer.start(10.0f);

    initiate();
}

slave_t::~slave_t() {
    syslog(LOG_DEBUG, "%s: destructing", identity());
    
    m_heartbeat_timer.stop();
    
    // TEST: Make sure that the slave is really dead
    BOOST_ASSERT(state_downcast<const dead*>() != 0);

    terminate();
}

void slave_t::react(const events::heartbeat_t& event) {
#if EV_VERSION_MAJOR == 3 && EV_VERSION_MINOR == 8
    if(!state_downcast<const alive*>()) {
        syslog(LOG_DEBUG, "%s: came alive in %.03f seconds",
            identity(), 
            10.0f - ev_timer_remaining(
                ev_default_loop(ev::AUTO),
                static_cast<ev_timer*>(&m_heartbeat_timer)
            )
        );
    }
#endif

    m_heartbeat_timer.stop();
    
    const busy* state = state_downcast<const busy*>();

    if(state && state->job()->policy().timeout > 0) {
        syslog(LOG_DEBUG, "%s: resetting timeout to %.02f seconds (job-specific)",
            identity(), state->job()->policy().timeout);
        m_heartbeat_timer.start(state->job()->policy().timeout);
    } else {
        syslog(LOG_DEBUG, "%s: resetting timeout to %.02f seconds", 
            identity(), config_t::get().engine.heartbeat_timeout);
        m_heartbeat_timer.start(config_t::get().engine.heartbeat_timeout);
    }
}

void slave_t::timeout(ev::timer&, int) {
    syslog(LOG_ERR, "%s: missed too many heartbeats", identity());
    
    const busy* state = state_downcast<const busy*>();
    
    if(state) {
        state->job()->process_event(events::error_t(
            client::timeout_error, "the job has timed out"));
    }
    
    process_event(events::terminated_t());
}

void alive::react(const events::invoked_t& event) {
    syslog(LOG_DEBUG, "%s: assigned a job", context<slave_t>().identity());
    m_job = event.job;
    m_job->process_event(event);
}

void alive::react(const events::choked_t& event) {
    syslog(LOG_DEBUG, "%s: job completed", context<slave_t>().identity());
    m_job->process_event(event);
    m_job.reset();
}

alive::~alive() {
    if(m_job && !m_job->state_downcast<const job::complete*>()) {
        syslog(LOG_INFO, "%s: rescheduling an incomplete '%s' job",
            context<slave_t>().identity(),
            m_job->driver()->method().c_str()
        );
       
        // NOTE: Allow the queue to grow beyond its capacity. 
        m_job->driver()->engine()->enqueue(m_job, true);
    }
}

dead::dead(my_context ctx):
    my_base(ctx)
{
    syslog(LOG_DEBUG, "%s: reaping", context<slave_t>().identity());
    context<slave_t>().reap();
}

