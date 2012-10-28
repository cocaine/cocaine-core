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

#include "cocaine/master.hpp"

#include "cocaine/context.hpp"
#include "cocaine/job.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/profile.hpp"

using namespace cocaine;
using namespace cocaine::engine;

master_t::master_t(context_t& context, ev::loop_ref& loop, const manifest_t& manifest, const profile_t& profile):
    state(state::unknown),
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % manifest.name
        ).str()
    )),
    m_loop(loop),
    m_heartbeat_timer(loop),
    m_manifest(manifest),
    m_profile(profile)
{
    // NOTE: Initialization heartbeat can be different.
    m_heartbeat_timer.set<master_t, &master_t::on_timeout>(this);
    m_heartbeat_timer.start(m_profile.startup_timeout);

    api::category_traits<api::isolate_t>::ptr_type isolate = m_context.get<api::isolate_t>(
        m_profile.isolate.type,
        api::category_traits<api::isolate_t>::args_type(
            m_manifest.name,
            m_profile.isolate.args
        )
    );

    std::map<std::string, std::string> args;

    args["--configuration"] = m_context.config.config_path;
    args["--slave:app"] = m_manifest.name;
    args["--slave:profile"] = m_profile.name;
    args["--slave:uuid"] = m_id.string();

    COCAINE_LOG_DEBUG(m_log, "spawning slave %s", m_id.string());

    m_handle = isolate->spawn(m_manifest.slave, args);
}

master_t::~master_t() {
    m_heartbeat_timer.stop();
    
    // TEST: Make sure that the slave is really dead.
    BOOST_ASSERT(state == state::dead);
}

const unique_id_t&
master_t::id() const {
    return m_id;
}

bool
master_t::operator==(const master_t& other) const {
    return m_id == other.m_id;
}

void
master_t::on_timeout(ev::timer&, int) {
    COCAINE_LOG_ERROR(
        m_log,
        "slave %s didn't respond in a timely fashion",
        m_id.string()
    );
    
    if(state == state::busy) {
        COCAINE_LOG_DEBUG(
            m_log,
            "slave %s dropping '%s' job due to a timeout",
            m_id.string(),
            job->event
        );

        job->process(
            events::error(
                timeout_error, 
                "the job has timed out"
            )
        );

        job->process(events::choke());
    }
    
    process(events::terminate());
}

void
master_t::process(const events::heartbeat& event) {
    if(state == state::unknown) {
        COCAINE_LOG_DEBUG(
            m_log,
            "slave %s came alive in %.03f seconds",
            m_id.string(),
            m_profile.startup_timeout - ev_timer_remaining(
                m_loop,
                static_cast<ev_timer*>(&m_heartbeat_timer)
            )
        );

        state = state::idle;
    }

    float timeout = m_profile.heartbeat_timeout;

    if(job && job->policy.timeout > 0.0f) {
        timeout = job->policy.timeout;
    }
           
    COCAINE_LOG_DEBUG(
        m_log,
        "resetting slave %s heartbeat timeout to %.02f seconds",
        m_id.string(),
        timeout
    );

    m_heartbeat_timer.stop();
    m_heartbeat_timer.start(timeout);
}

void
master_t::process(const events::terminate& event) {
    // Ensure that the slave is not being overkilled.
    BOOST_ASSERT(state != state::dead);

    COCAINE_LOG_DEBUG(m_log, "reaping slave %s", m_id.string());

    if(job) {
        job->process(
            events::error(
                server_error,
                "the job is being cancelled"
            )
        );

        job->process(events::choke());
        
        job.reset();
    }

    m_handle->terminate();
    m_handle.reset();

    state = state::dead;
}

void
master_t::process(const events::invoke& event) {
    // TEST: Ensure that no job is being lost here.
    BOOST_ASSERT(state == state::idle && !job && event.job);

    COCAINE_LOG_DEBUG(
        m_log,
        "job '%s' assigned to slave %s",
        event.job->event,
        m_id.string()
    );

    job = event.job;
    job->process(event);    
    
    state = state::busy;

    // Reset the heartbeat timer.    
    process(events::heartbeat());
}

void
master_t::process(const events::choke& event) {
    // TEST: Ensure that the job is in fact here.
    BOOST_ASSERT(state == state::busy && job);

    COCAINE_LOG_DEBUG(
        m_log,
        "job '%s' completed by slave %s",
        job->event,
        m_id.string()
    );
    
    job->process(event);
    job.reset();

    state = state::idle;
    
    // Reset the heartbeat timer.    
    process(events::heartbeat());
}

void
master_t::process(const events::chunk& event) {
    // TEST: Ensure that the job is in fact here.
    BOOST_ASSERT(state == state::busy && job);

    job->process(event);
    
    // Reset the heartbeat timer.    
    process(events::heartbeat());
}

void
master_t::process(const events::error& event) {
    if(state == state::busy) {
        // TEST: Ensure that the job is in fact here.
        BOOST_ASSERT(job);

        job->process(event);
    }
        
    // Reset the heartbeat timer.    
    process(events::heartbeat());
}
