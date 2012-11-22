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
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/profile.hpp"
#include "cocaine/session.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/api/isolate.hpp"
#include "cocaine/api/stream.hpp"

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
        m_context,
        m_manifest.name,
        m_profile.isolate.args
    );

    std::map<std::string, std::string> args,
                                       environment;

    args["--configuration"] = m_context.config.config_path;
    args["--slave:app"] = m_manifest.name;
    args["--slave:profile"] = m_profile.name;
    args["--slave:uuid"] = m_id.string();

    COCAINE_LOG_DEBUG(m_log, "slave %s spawning", m_id);

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
        "slave %s has started processing session %s",
        m_id,
        session->id
    );

    m_sessions.emplace(session->id, session);

    // This will potentially flood the slave with the cached messages.
    session->attach(this);

    if(m_idle_timer.is_active()) {
        m_idle_timer.stop();
    }
}

void
slave_t::on_ping() {
    rearm();

    // Rearming first so that slave will be in a correct state.
    send<rpc::pong>();
}

void
slave_t::on_chunk(const unique_id_t& session_id,
                  const std::string& message)
{
    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s received session %s chunk, size: %llu bytes",
        m_id,
        session_id,
        message.size()
    );

    session_map_t::iterator it(m_sessions.find(session_id));

    // TEST: Ensure that this slave is responsible for the session.
    BOOST_ASSERT(it != m_sessions.end());

    it->second->upstream->push(message.data(), message.size());
}

void
slave_t::on_error(const unique_id_t& session_id,
                  error_code code,
                  const std::string& message)
{
    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s received session %s error, code: %d, message: %s",
        m_id,
        session_id,
        code,
        message
    );

    session_map_t::iterator it(m_sessions.find(session_id));

    // TEST: Ensure that this slave is responsible for the session.
    BOOST_ASSERT(it != m_sessions.end());

    it->second->upstream->error(code, message);
}

void
slave_t::on_choke(const unique_id_t& session_id) {
    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s has completed session %s",
        m_id,
        session_id
    );

    session_map_t::iterator it(m_sessions.find(session_id));

    // TEST: Ensure that this slave is responsible for the session.
    BOOST_ASSERT(it != m_sessions.end());

    it->second->upstream->close();

    // NOTE: As we're destroying the session here, we have to close the
    // downstream, otherwise the client wouldn't be able to close it later.
    // it->second->send<rpc::choke>();

    m_sessions.erase(it);

    if(m_sessions.empty()) {
        m_idle_timer.start(m_profile.idle_timeout);
    }
}

namespace {
    struct timeout_t {
        template<class T>
        void
        operator()(T& session) {
            session.second->abandon(
                timeout_error, 
                "the session has timed out"
            );
        }
    };
}

void
slave_t::on_timeout(ev::timer&, int) {
    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s has timed out, dropping %llu sessions",
        m_id,
        m_sessions.size()
    );

    std::for_each(m_sessions.begin(), m_sessions.end(), timeout_t());
    
    m_sessions.clear();
    
    terminate();
}

void
slave_t::on_idle(ev::timer&, int) {
    send<rpc::terminate>();

    // Inactive slaves won't get any new sessions.
    m_state = states::inactive;
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

        m_state = states::active;

        // Start the idle timer, which will kill the slave when it's not used.
        m_idle_timer.set<slave_t, &slave_t::on_idle>(this);
        m_idle_timer.start(m_profile.idle_timeout);
    }

    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s resetting heartbeat timeout to %.02f seconds",
        m_id,
        m_profile.heartbeat_timeout
    );

    m_heartbeat_timer.stop();
    m_heartbeat_timer.start(m_profile.heartbeat_timeout);
}

void
slave_t::terminate() {
    COCAINE_LOG_DEBUG(m_log, "slave %s terminating", m_id);

    // Ensure that the slave is not being overkilled.
    BOOST_ASSERT(m_state != states::dead);

    // Ensure that no session is being lost here.
    BOOST_ASSERT(m_sessions.empty());

    m_heartbeat_timer.stop();
    m_idle_timer.stop();

    m_handle->terminate();
    m_handle.reset();

    m_state = states::dead;
}

