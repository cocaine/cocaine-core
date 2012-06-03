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

#include <boost/assign.hpp>
#include <sys/wait.h>
#include <unistd.h>

#include "cocaine/slave.hpp"

#include "cocaine/app.hpp"
#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/job.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::slave;

slave_t::slave_t(context_t& context, engine_t& engine):
    m_context(context),
    m_engine(engine),
    m_heartbeat_timer(m_engine.loop())
{
    // NOTE: These are the 10 seconds for the slave to come alive.
    m_heartbeat_timer.set<slave_t, &slave_t::on_timeout>(this);
    m_heartbeat_timer.start(10.0f);

    initiate();
    spawn();
}

slave_t::~slave_t() {
    m_heartbeat_timer.stop();
    
    // TEST: Make sure that the slave is really dead.
    BOOST_ASSERT(state_downcast<const dead*>() != 0);

    terminate();
}

bool slave_t::operator==(const slave_t& other) const {
    return id() == other.id();
}

namespace {
    typedef std::map<
        sc::event_base::id_type,
        std::string
    > event_names_t;

    event_names_t names = boost::assign::map_list_of
        (events::heartbeat::static_type(), "heartbeat")
        (events::terminate::static_type(), "terminate")
        (events::invoke::static_type(), "invoke")
        (events::chunk::static_type(), "chunk")
        (events::error::static_type(), "error")
        (events::choke::static_type(), "choke");
}

void slave_t::unconsumed_event(const sc::event_base& event) {
    event_names_t::const_iterator it(names.find(event.dynamic_type()));

    // TEST: Unconsumed rogue event is a fatal error.
    BOOST_ASSERT(it != names.end());

    m_engine.app().log->warning(
        "slave %s detected an unconsumed '%s' event",
        id().c_str(),
        it->second.c_str()
    );
}

void slave_t::spawn() {
    m_pid = ::fork();

    if(m_pid == 0) {
        int rv = 0;

#ifdef HAVE_CGROUPS
        if(m_engine.group()) {
            if((rv = cgroup_attach_task(m_engine.group())) != 0) {
                m_engine.app().log->error(
                    "unable to attach slave %s to a control group - %s",
                    id().c_str(),
                    cgroup_strerror(rv)
                );

                std::exit(EXIT_FAILURE);
            }
        }
#endif

        rv = ::execl(
            m_context.config.runtime.self.c_str(),
            m_context.config.runtime.self.c_str(),
            "--slave:app", m_engine.app().name().c_str(),
            "--slave:uuid",  id().c_str(),
            "--configuration", m_context.config.config_path.c_str(),
            (char*)0
        );

        if(rv != 0) {
            char buffer[1024];

#ifdef _GNU_SOURCE
            char * message;
            message = ::strerror_r(errno, buffer, 1024);
#else
            ::strerror_r(errno, buffer, 1024);
#endif

            m_engine.app().log->error(
                "unable to start slave %s - %s",
                id().c_str(),
#ifdef _GNU_SOURCE
                message
#else
                buffer
#endif
            );

            std::exit(EXIT_FAILURE);
        }
    } else if(m_pid < 0) {
        throw system_error_t("fork() failed");
    }
}

void slave_t::on_initialize(const events::heartbeat& event) {
#if EV_VERSION_MAJOR == 3 && EV_VERSION_MINOR == 8
    m_engine.app().log->debug(
        "slave %s came alive in %.03f seconds",
        id().c_str(),
        10.0f - ev_timer_remaining(
            m_engine.loop(),
            static_cast<ev_timer*>(&m_heartbeat_timer)
        )
    );
#endif

    on_heartbeat(event);
}

void slave_t::on_heartbeat(const events::heartbeat& event) {
    m_heartbeat_timer.stop();
    
    const busy * state = state_downcast<const busy*>();
    float timeout = m_engine.app().policy.heartbeat_timeout;

    if(state && state->job()->policy.timeout > 0.0f) {
        timeout = state->job()->policy.timeout;
    }
           
    m_engine.app().log->debug(
        "resetting slave %s heartbeat timeout to %.02f seconds",
        id().c_str(),
        timeout
    );

    m_heartbeat_timer.start(timeout);
}

void slave_t::on_terminate(const events::terminate& event) {
    m_engine.app().log->debug(
        "reaping slave %s", 
        id().c_str()
    );

    int status = 0;

    if(waitpid(m_pid, &status, WNOHANG) == 0) {
        ::kill(m_pid, SIGTERM);
    }
}

void slave_t::on_timeout(ev::timer&, int) {
    m_engine.app().log->error(
        "slave %s doesn't respond in a timely fashion",
        id().c_str()
    );
    
    const busy * state = state_downcast<const busy*>();

    if(state) {
        state->job()->process_event(
            events::error(
                dealer::timeout_error, 
                "the job has timed out"
            )
        );
    }
    
    process_event(events::terminate());
}

void alive::on_invoke(const events::invoke& event) {
    // TEST: Ensure that no job is being lost here.
    BOOST_ASSERT(!job && event.job);

    context<slave_t>().m_engine.app().log->debug(
        "job '%s' assigned to slave %s",
        event.job->event.c_str(),
        context<slave_t>().id().c_str()
    );

    job = event.job;
    job->process_event(event);    
}

void alive::on_choke(const events::choke& event) {
    // TEST: Ensure that the job is in fact here.
    BOOST_ASSERT(job);

    context<slave_t>().m_engine.app().log->debug(
        "job '%s' completed by slave %s",
        job->event.c_str(),
        context<slave_t>().id().c_str()
    );
    
    job->process_event(event);
    job.reset();
}

alive::~alive() {
    if(job && !job->state_downcast<const job::complete*>()) {
        context<slave_t>().m_engine.app().log->warning(
            "trying to reschedule an incomplete '%s' job",
            job->event.c_str()
        );

        context<slave_t>().m_engine.enqueue(job);
    }
}

void busy::on_chunk(const events::chunk& event) {
    job()->process_event(event);
    post_event(events::heartbeat());
}

void busy::on_error(const events::error& event) {
    job()->process_event(event);
    post_event(events::heartbeat());
}
