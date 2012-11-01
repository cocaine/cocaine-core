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
#include "cocaine/engine.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/profile.hpp"
#include "cocaine/rpc.hpp"
#include "cocaine/session.hpp"

#include "cocaine/api/job.hpp"

#include "cocaine/traits/unique_id.hpp" 

using namespace cocaine;
using namespace cocaine::engine;

master_t::master_t(context_t& context,
                   const manifest_t& manifest,
                   const profile_t& profile,
                   engine_t * engine):
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % manifest.name
        ).str()
    )),
    m_manifest(manifest),
    m_profile(profile),
    m_heartbeat_timer(engine->loop()),
    m_state(state::unknown)
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

    std::map<std::string, std::string> args,
                                       environment;

    args["--configuration"] = m_context.config.config_path;
    args["--slave:app"] = m_manifest.name;
    args["--slave:profile"] = m_profile.name;
    args["--slave:uuid"] = m_id.string();

    COCAINE_LOG_DEBUG(m_log, "spawning slave %s", m_id);

    m_handle = isolate->spawn(m_manifest.slave, args, environment);
}

master_t::~master_t() {
    m_heartbeat_timer.stop();
    
    // TEST: Make sure that the slave is really dead.
    BOOST_ASSERT(m_state == state::dead);
}

void
master_t::process(const events::heartbeat& event) {
    if(m_state == state::unknown) {
        COCAINE_LOG_DEBUG(
            m_log,
            "slave %s came alive in %.03f seconds",
            m_id,
            m_profile.startup_timeout - ev_timer_remaining(
                m_engine->loop(),
                static_cast<ev_timer*>(&m_heartbeat_timer)
            )
        );

        m_state = state::idle;
    }

    float timeout = m_profile.heartbeat_timeout;

    if(m_state == state::busy && session->job->policy.timeout > 0.0f) {
        timeout = session->job->policy.timeout;
    }

    COCAINE_LOG_DEBUG(
        m_log,
        "resetting slave %s heartbeat timeout to %.02f seconds",
        m_id,
        timeout
    );

    m_heartbeat_timer.stop();
    m_heartbeat_timer.start(timeout);
}

void
master_t::process(const events::terminate& event) {
    COCAINE_LOG_DEBUG(m_log, "terminating slave %s", m_id);

    // Ensure that the slave is not being overkilled.
    BOOST_ASSERT(m_state != state::dead);

    // Ensure that no session is being lost here.
    BOOST_ASSERT(!session);

    m_handle->terminate();
    m_handle.reset();

    m_state = state::dead;
}

void
master_t::process(const events::invoke& event) {
    COCAINE_LOG_DEBUG(
        m_log,
        "session %s assigned to slave %s",
        session->id,
        m_id
    );

    // TEST: Ensure that no session is being lost here.
    BOOST_ASSERT(m_state == state::idle && !session);
    
    // TEST: Ensure that the event has a bound session.
    BOOST_ASSERT(event.session);

    session = event.session;
    session->process(event);
    
    m_state = state::busy;

    // Reset the heartbeat timer.    
    process(events::heartbeat());
}

void
master_t::process(const events::choke& event) {
    COCAINE_LOG_DEBUG(
        m_log,
        "session %s completed by slave %s",
        session->id,
        m_id
    );
    
    // TEST: Ensure that the session is in fact here.
    BOOST_ASSERT(m_state == state::busy && session);

    session->process(event);
    session.reset();

    m_state = state::idle;
    
    // Reset the heartbeat timer.    
    process(events::heartbeat());
}

void
master_t::process(const events::chunk& event) {
    // TEST: Ensure that the session is in fact here.
    BOOST_ASSERT(m_state == state::busy && session);

    session->process(event);
    
    // Reset the heartbeat timer.    
    process(events::heartbeat());
}

void
master_t::process(const events::error& event) {
    if(m_state == state::busy) {
        // TEST: Ensure that the session is in fact here.
        BOOST_ASSERT(session);

        session->process(event);
    }
        
    // Reset the heartbeat timer.    
    process(events::heartbeat());
}

void
master_t::push(const std::string& chunk) {
    zmq::message_t message(chunk.size());

    memcpy(
        message.data(),
        chunk.data(),
        chunk.size()
    );

    io::message<rpc::chunk> command(message);

    m_engine->send(
        m_id,
        command
    );
}

void
master_t::on_timeout(ev::timer&, int) {
    if(m_state == state::busy) {
        session->process(
            events::error(
                timeout_error, 
                "the session has timed out"
            )
        );

        session.reset();
    }

    process(events::terminate());
}
