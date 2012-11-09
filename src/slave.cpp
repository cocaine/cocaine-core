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

#include "cocaine/slave.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/profile.hpp"
#include "cocaine/session.hpp"

#include "cocaine/api/event.hpp"

#include "cocaine/traits/unique_id.hpp" 

using namespace cocaine;
using namespace cocaine::engine;

slave_t::slave_t(context_t& context,
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
    m_engine(engine),
    m_heartbeat_timer(engine->loop()),
    m_state(states::unknown)
{
    api::category_traits<api::isolate_t>::ptr_type isolate = m_context.get<api::isolate_t>(
        m_profile.isolate.type,
        m_manifest.name,
        m_profile.isolate.args
    );

    std::map<std::string, std::string> args,
                                       environment;

    args["--configuration"] = m_context.config.config_path;
    args["--slave:app"] = m_manifest.name;
    args["--slave:profile"] = m_profile.name;
    args["--slave:uuid"] = m_id.string();

    COCAINE_LOG_DEBUG(m_log, "spawning slave %s", m_id);

    m_handle = isolate->spawn(m_manifest.slave, args, environment);

    // NOTE: Initialization heartbeat can be different.
    m_heartbeat_timer.set<slave_t, &slave_t::on_timeout>(this);
    m_heartbeat_timer.start(m_profile.startup_timeout);
}

slave_t::~slave_t() {    
    if(m_state != states::dead) {
        terminate();
    }
}

void
slave_t::assign(const boost::shared_ptr<session_t>& session) {
    COCAINE_LOG_DEBUG(
        m_log,
        "session %s assigned to slave %s",
        session->id,
        m_id
    );

    // TEST: Ensure that no session is being lost here.
    BOOST_ASSERT(m_state == states::idle && !m_session);
    
    m_session = session;
    m_session->attach(this);
    
    m_state = states::busy;
}

void
slave_t::process(const io::message<rpc::ping>&) {
    m_engine->send(
        m_id,
        io::message<rpc::pong>()
    );

    rearm();
}

void
slave_t::process(const io::message<rpc::chunk>& m) {
    // TEST: Ensure that the session is in fact here.
    BOOST_ASSERT(m_state == states::busy && m_session);

    zmq::message_t& message = boost::get<0>(m);

    COCAINE_LOG_DEBUG(
        m_log,
        "session %s sent a chunk from slave %s, size: %llu bytes",
        m_session->id,
        m_id,
        message.size()
    );

    if(m_session->state == session_t::states::active) {
        m_session->ptr->on_chunk(message.data(), message.size());
        rearm();
    }
}

void
slave_t::process(const io::message<rpc::error>& m) {
    if(m_state != states::busy) {
        // NOTE: This means that the slave has failed to start and reported
        // an error, without having a session assigned yet.
        return;
    }

    // TEST: Ensure that the session is in fact here.
    BOOST_ASSERT(m_session);

    error_code code = static_cast<error_code>(boost::get<0>(m));
    const std::string& message = boost::get<1>(m);

    COCAINE_LOG_DEBUG(
        m_log,
        "session %s sent an error from slave %s, code: %d, message: %s",
        m_session->id,
        m_id,
        code,
        message
    );

    if(m_session->state == session_t::states::active) {
        m_session->ptr->on_error(code, message);
        rearm();
    }
}

void
slave_t::process(const io::message<rpc::choke>&) {
    // TEST: Ensure that the session is in fact here.
    BOOST_ASSERT(m_state == states::busy && m_session);
    
    COCAINE_LOG_DEBUG(
        m_log,
        "session %s completed by slave %s",
        m_session->id,
        m_id
    );

    if(m_session->state == session_t::states::active) {
        m_session->ptr->on_close();
        m_session->detach();
    }

    m_session.reset();

    m_state = states::idle;
    
    rearm();
}

void
slave_t::on_timeout(ev::timer&, int) {
    if(m_state == states::busy) {
        m_session->ptr->on_error(
            timeout_error, 
            "the session has timed out"
        );

        m_session.reset();
    }

    terminate();
}

void
slave_t::rearm() {
    if(m_state == states::unknown) {
        COCAINE_LOG_DEBUG(
            m_log,
            "slave %s came alive in %.03f seconds",
            m_id,
            m_profile.startup_timeout - ev_timer_remaining(
                m_engine->loop(),
                static_cast<ev_timer*>(&m_heartbeat_timer)
            )
        );

        m_state = states::idle;
    }

    float timeout = m_profile.heartbeat_timeout;

    if(m_state == states::busy && m_session->ptr->policy.timeout > 0.0f) {
        timeout = m_session->ptr->policy.timeout;
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
slave_t::terminate() {
    COCAINE_LOG_DEBUG(m_log, "terminating slave %s", m_id);

    // Ensure that the slave is not being overkilled.
    BOOST_ASSERT(m_state != states::dead);

    // Ensure that no session is being lost here.
    BOOST_ASSERT(!m_session);

    m_heartbeat_timer.stop();

    m_handle->terminate();
    m_handle.reset();

    m_state = states::dead;
}
