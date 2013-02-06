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

#include "cocaine/slave.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/io.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/profile.hpp"
#include "cocaine/rpc.hpp"
#include "cocaine/session.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/traits/unique_id.hpp" 

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;
using namespace cocaine::logging;

handshake_t::handshake_t(engine_t& engine,
                         const boost::shared_ptr<pipe_t>& pipe):
    m_engine(engine),
    m_codex(new codex<pipe_t>(engine.loop(), pipe))
{
    m_codex->bind(boost::bind(&handshake_t::on_message, this, _1));
}

void
handshake_t::on_message(const message_t& message) {
    unique_id_t id;

    // Unpack the handshake.
    message.as<rpc::handshake>(id);

    std::cout << "binding codex for " << id.string() << std::endl;

    m_engine.tie(m_codex, id);
}

slave_t::slave_t(context_t& context,
                 const manifest_t& manifest,
                 const profile_t& profile,
                 engine_t& engine):
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%s", manifest.name))),
    m_manifest(manifest),
    m_profile(profile),
    m_engine(engine),
    m_state(state_t::unknown),
    m_heartbeat_timer(engine.loop()),
    m_idle_timer(engine.loop())
{
    auto isolate = m_context.get<api::isolate_t>(
        m_profile.isolate.type,
        m_context,
        m_manifest.name,
        m_profile.isolate.args
    );

    std::map<std::string, std::string> args,
                                       environment;

    args["-c"] = m_context.config.path.config;
    args["--app"] = m_manifest.name;
    args["--profile"] = m_profile.name;
    args["--uuid"] = m_id.string();

    COCAINE_LOG_DEBUG(m_log, "slave %s is activating", m_id);

    m_handle = isolate->spawn(m_manifest.slave, args, environment);

    // NOTE: Initialization heartbeat can be different.
    m_heartbeat_timer.set<slave_t, &slave_t::on_timeout>(this);
    m_heartbeat_timer.start(m_profile.startup_timeout);
}

slave_t::~slave_t() {    
    if(m_state != state_t::dead) {
        terminate();
    }
}

void
slave_t::tie(const boost::shared_ptr<io::codex<io::pipe_t>>& codex) {
    COCAINE_LOG_DEBUG(m_log, "slave %s was tied to a worker", m_id);

    m_codex = codex;
    m_codex->bind(boost::bind(&slave_t::on_message, this, _1));

    on_ping();

    m_engine.pump();
}

void
slave_t::assign(boost::shared_ptr<session_t>&& session) {
    BOOST_ASSERT(m_state == state_t::active);

    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s has started processing session %s",
        m_id,
        session->id
    );

    session->attach(this);

    m_sessions.insert(
        std::make_pair(session->id, std::move(session))
    );

    if(m_idle_timer.is_active()) {
        m_idle_timer.stop();
    }
}

void
slave_t::on_message(const message_t& message) {
    COCAINE_LOG_DEBUG(
        m_log,
        "received type %d message from slave %s",
        message.id(),
        m_id
    );

    switch(message.id()) {
        case event_traits<rpc::heartbeat>::id:
            on_ping();
            break;

        /*
        case event_traits<rpc::suicide>::id: {
            int code;
            std::string reason;

            message.as<rpc::suicide>(code, reason);

            COCAINE_LOG_DEBUG(
                m_log,
                "slave %s is committing suicide: %s",
                slave_id,
                reason
            );

            m_pool.erase(slave);

            if(code == rpc::suicide::abnormal) {
                COCAINE_LOG_ERROR(m_log, "the app seems to be broken - stopping");
                migrate(state_t::broken);
                return;
            }

            if(m_state != state_t::running && m_pool.empty()) {
                // If it was the last slave, shut the engine down.
                stop();
                return;
            }

            break;
        }
        */

        case event_traits<rpc::chunk>::id: {
            uint64_t session_id;
            std::string chunk;
            
            message.as<rpc::chunk>(session_id, chunk);

            on_chunk(session_id, chunk);

            break;
        }
     
        case event_traits<rpc::error>::id: {
            uint64_t session_id;
            int code;
            std::string reason;

            message.as<rpc::error>(session_id, code, reason);
            
            on_error(
                session_id,
                static_cast<error_code>(code),
                reason
            );

            break;
        }

        case event_traits<rpc::choke>::id: {
            uint64_t session_id;

            message.as<rpc::choke>(session_id);

            on_choke(session_id);

            break;
        }

        default:
            COCAINE_LOG_WARNING(
                m_log,
                "dropping unknown type %d message from slave %s",
                message.id(),
                m_id
            );
    }
}

void
slave_t::on_ping() {
    BOOST_ASSERT(m_state != state_t::dead);

    if(m_state == state_t::unknown) {
        COCAINE_LOG_DEBUG(
            m_log,
            "slave %s became active in %.03f seconds",
            m_id,
            m_profile.startup_timeout - ev_timer_remaining(
                m_engine.loop(),
                static_cast<ev_timer*>(&m_heartbeat_timer)
            )
        );

        m_state = state_t::active;

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

    send(io::codec::pack<rpc::heartbeat>());
}

void
slave_t::on_chunk(uint64_t session_id,
                  const std::string& chunk)
{
    BOOST_ASSERT(m_state == state_t::active);
    
    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s received session %s chunk, size: %llu bytes",
        m_id,
        session_id,
        chunk.size()
    );

    session_map_t::iterator it(m_sessions.find(session_id));

    // TEST: Ensure that this slave is responsible for the session.
    BOOST_ASSERT(it != m_sessions.end());

    it->second->upstream->push(chunk.data(), chunk.size());
}

void
slave_t::on_error(uint64_t session_id,
                  error_code code,
                  const std::string& reason)
{
    BOOST_ASSERT(m_state == state_t::active);
    
    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s received session %s error, code: %d, message: %s",
        m_id,
        session_id,
        code,
        reason
    );

    session_map_t::iterator it(m_sessions.find(session_id));

    // TEST: Ensure that this slave is responsible for the session.
    BOOST_ASSERT(it != m_sessions.end());

    it->second->upstream->error(code, reason);
}

void
slave_t::on_choke(uint64_t session_id) {
    BOOST_ASSERT(m_state == state_t::active);
    
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
    // TODO: Think about it.
    it->second->send<rpc::choke>();
    it->second->detach();

    m_sessions.erase(it);

    if(m_sessions.empty()) {
        m_idle_timer.start(m_profile.idle_timeout);
    }
}

void
slave_t::send(const std::string& blob) {
    BOOST_ASSERT(m_state == state_t::active);
    m_codex->send(blob);
}

namespace {
    struct timeout_t {
        template<class T>
        void
        operator()(T& session) const {
            session.second->upstream->error(
                timeout_error, 
                "the session has timed out"
            );
        }
    };
}

void
slave_t::on_timeout(ev::timer&, int) {
    BOOST_ASSERT(m_state != state_t::dead);
    
    switch(m_state) {
        case state_t::unknown:
            COCAINE_LOG_WARNING(m_log, "slave %s has failed to activate", m_id);
            break;

        case state_t::active:
            COCAINE_LOG_WARNING(
                m_log,
                "slave %s has timed out, dropping %llu sessions",
                m_id,
                m_sessions.size()
            );

            std::for_each(m_sessions.begin(), m_sessions.end(), timeout_t());

            m_sessions.clear();

            break;

        case state_t::inactive:
            COCAINE_LOG_WARNING(m_log, "slave %s has failed to deactivate", m_id);
            break;

        case state_t::dead:
            // NOTE: Unreachable.
            std::terminate();
    }
    
    terminate();
}

void
slave_t::on_idle(ev::timer&, int) {
    BOOST_ASSERT(m_state == state_t::active);

    COCAINE_LOG_DEBUG(m_log, "slave %s is idle, deactivating", m_id);

    send(io::codec::pack<rpc::terminate>());

    m_state = state_t::inactive;
}

void
slave_t::terminate() {
    COCAINE_LOG_DEBUG(m_log, "slave %s terminating", m_id);

    // Ensure that the slave is not being overkilled.
    BOOST_ASSERT(m_state != state_t::dead);

    // Ensure that no sessions are being lost here.
    BOOST_ASSERT(m_sessions.empty());

    m_heartbeat_timer.stop();
    m_idle_timer.stop();

    m_handle->terminate();
    m_handle.reset();

    m_state = state_t::dead;
}

