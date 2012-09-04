/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#include <boost/format.hpp>
#include <sys/wait.h>
#include <unistd.h>

#include "cocaine/master.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/job.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::slave;

master_t::master_t(context_t& context, engine_t& engine):
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % engine.manifest().name
        ).str()
    )),
    m_engine(engine),
    m_heartbeat_timer(m_engine.loop())
{
    initiate();
    
    // NOTE: Initialization heartbeat can be different.
    m_heartbeat_timer.set<master_t, &master_t::on_timeout>(this);
    m_heartbeat_timer.start(m_engine.manifest().policy.startup_timeout);

    spawn();
}

master_t::~master_t() {
    m_heartbeat_timer.stop();
    
    // TEST: Make sure that the slave is really dead.
    BOOST_ASSERT(state_downcast<const dead*>() != 0);

    terminate();
}

bool master_t::operator==(const master_t& other) const {
    return id() == other.id();
}

void master_t::spawn() {
    m_log->debug(
        "spawning slave %s",
        id().c_str()
    );

    m_pid = ::fork();

    if(m_pid == 0) {
        int rv = 0;

#ifdef HAVE_CGROUPS
        if(m_engine.group()) {
            if((rv = cgroup_attach_task(m_engine.group())) != 0) {
                m_log->error(
                    "unable to attach slave %s to the control group - %s",
                    id().c_str(),
                    cgroup_strerror(rv)
                );

                std::exit(EXIT_FAILURE);
            }
        }
#endif

        rv = ::execlp(
            m_engine.manifest().slave.c_str(),
            m_engine.manifest().slave.c_str(),
            "--slave:app", m_engine.manifest().name.c_str(),
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

            m_log->error(
                "unable to execute '%s' - %s",
                m_engine.manifest().slave.c_str(),
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

void master_t::on_initialize(const events::heartbeat& event) {
#if EV_VERSION_MAJOR == 3 && EV_VERSION_MINOR == 8
    m_log->debug(
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

void master_t::on_heartbeat(const events::heartbeat& event) {
    m_heartbeat_timer.stop();
    
    const busy * state = state_downcast<const busy*>();
    float timeout = m_engine.manifest().policy.heartbeat_timeout;

    if(state && state->job()->policy.timeout > 0.0f) {
        timeout = state->job()->policy.timeout;
    }
           
    m_log->debug(
        "resetting slave %s heartbeat timeout to %.02f seconds",
        id().c_str(),
        timeout
    );

    m_heartbeat_timer.start(timeout);
}

void master_t::on_terminate(const events::terminate& event) {
    m_log->debug(
        "reaping slave %s", 
        id().c_str()
    );

    int status = 0;

    if(waitpid(m_pid, &status, WNOHANG) == 0) {
        ::kill(m_pid, SIGTERM);
    }
}

void master_t::on_timeout(ev::timer&, int) {
    m_log->error(
        "slave %s didn't respond in a timely fashion",
        id().c_str()
    );
    
    const busy * state = state_downcast<const busy*>();

    if(state) {
        m_log->debug(
            "slave %s dropping '%s' job due to a timeout",
            id().c_str(),
            state->job()->event.c_str()
        );

        state->job()->process_event(
            events::error(
                timeout_error, 
                "the job has timed out"
            )
        );
    }
    
    process_event(events::terminate());
}

alive::~alive() {
    if(job) {
        job->process_event(
            events::error(
                server_error,
                "the job is being cancelled"
            )
        );

        job.reset();
    }
}

void alive::on_invoke(const events::invoke& event) {
    // TEST: Ensure that no job is being lost here.
    BOOST_ASSERT(!job && event.job);

    context<master_t>().m_log->debug(
        "job '%s' assigned to slave %s",
        event.job->event.c_str(),
        context<master_t>().id().c_str()
    );

    job = event.job;
    job->process_event(event);    

    // Reset the heartbeat timer.    
    post_event(events::heartbeat());
}

void alive::on_choke(const events::choke& event) {
    // TEST: Ensure that the job is in fact here.
    BOOST_ASSERT(job);

    context<master_t>().m_log->debug(
        "job '%s' completed by slave %s",
        job->event.c_str(),
        context<master_t>().id().c_str()
    );
    
    job->process_event(event);
    job.reset();
    
    // Reset the heartbeat timer.    
    post_event(events::heartbeat());
}

void busy::on_chunk(const events::chunk& event) {
    job()->process_event(event);
    
    // Reset the heartbeat timer.    
    post_event(events::heartbeat());
}

void busy::on_error(const events::error& event) {
    job()->process_event(event);
    
    // Reset the heartbeat timer.    
    post_event(events::heartbeat());
}
