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

#include "cocaine/detail/slave.hpp"

#include "cocaine/context.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/detail/engine.hpp"
#include "cocaine/detail/manifest.hpp"
#include "cocaine/detail/profile.hpp"
#include "cocaine/detail/session.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;
using namespace cocaine::logging;

using namespace std::placeholders;

slave_t::slave_t(context_t& context,
                 reactor_t& reactor,
                 const manifest_t& manifest,
                 const profile_t& profile,
                 engine_t& engine):
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%s", manifest.name))),
    m_manifest(manifest),
    m_profile(profile),
    m_engine(engine),
    m_state(states::unknown),
#if defined(__clang__) || defined(HAVE_GCC47)
    m_birthstamp(std::chrono::steady_clock::now()),
#else
    m_birthstamp(std::chrono::monotonic_clock::now()),
#endif
    m_heartbeat_timer(reactor.native()),
    m_idle_timer(reactor.native())
{
    // NOTE: Initialization heartbeat can be different.
    m_heartbeat_timer.set<slave_t, &slave_t::on_timeout>(this);
    m_heartbeat_timer.start(m_profile.startup_timeout);

    COCAINE_LOG_DEBUG(m_log, "slave %s is activating", m_id);

    auto isolate = m_context.get<api::isolate_t>(
        m_profile.isolate.type,
        m_context,
        m_manifest.name,
        m_profile.isolate.args
    );

    typedef std::map<std::string, std::string> string_map_t;

    string_map_t args;
    string_map_t environment;

    args["--app"] = m_manifest.name;
    args["--endpoint"] = m_manifest.endpoint;
    args["--uuid"] = m_id.string();

    COCAINE_LOG_DEBUG(m_log, "slave %s spawning '%s'", m_id, m_manifest.slave);

    m_handle = isolate->spawn(m_manifest.slave, args, environment);
}

slave_t::~slave_t() {
    if(m_state != states::dead) {
        terminate();
    }
}

void
slave_t::bind(const std::shared_ptr<channel<io::socket<local>>>& channel_) {
    m_channel = channel_;

    m_channel->rd->bind(
        std::bind(&slave_t::on_message, this, _1),
        std::bind(&slave_t::on_disconnect, this, _1)
    );

    m_channel->wr->bind(
        std::bind(&slave_t::on_disconnect, this, _1)
    );

    on_ping();
}

void
slave_t::assign(std::shared_ptr<session_t>&& session) {
    BOOST_ASSERT(m_state == states::active);

    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s has started processing session %s",
        m_id,
        session->id
    );

    session->attach(m_channel->wr->stream());

    m_sessions.insert(
        std::make_pair(session->id, std::move(session))
    );

    if(m_idle_timer.is_active()) {
        m_idle_timer.stop();
    }
}

void
slave_t::stop() {
    BOOST_ASSERT(m_channel);

    m_channel->wr->write<rpc::terminate>(
        0UL,
        static_cast<int>(rpc::terminate::normal),
        std::string("shutdown")
    );
}

void
slave_t::on_message(const message_t& message) {
    COCAINE_LOG_DEBUG(
        m_log,
        "received type %d message in band %d from slave %s",
        message.id(),
        message.band(),
        m_id
    );

    switch(message.id()) {
        case event_traits<rpc::heartbeat>::id:
            on_ping();
            break;

        case event_traits<rpc::terminate>::id: {
            int code;
            std::string reason;

            message.as<rpc::terminate>(code, reason);
            on_death(code, reason);

            break;
        }

        case event_traits<rpc::chunk>::id: {
            std::string chunk;

            message.as<rpc::chunk>(chunk);
            on_chunk(message.band(), chunk);

            break;
        }

        case event_traits<rpc::error>::id: {
            int code;
            std::string reason;

            message.as<rpc::error>(code, reason);
            on_error(message.band(), code, reason);

            break;
        }

        case event_traits<rpc::choke>::id: {
            on_choke(message.band());

            // This is now a potentially free worker, so pump the queue.
            m_engine.wake();

            break;
        }

        default:
            COCAINE_LOG_WARNING(
                m_log,
                "dropping unknown type %d message in band %d from slave %s",
                message.id(),
                message.band(),
                m_id
            );
    }
}

namespace {
    struct cancel_t {
        cancel_t(error_code code, std::string message):
            m_code(code),
            m_message(message)
        { }

        template<class T>
        void
        operator()(const T& session) const {
            session.second->detach();
            session.second->upstream->error(m_code, m_message);
        }

    private:
        const error_code m_code;
        const std::string m_message;
    };
}

void
slave_t::on_disconnect(const std::error_code& ec) {
    COCAINE_LOG_WARNING(
        m_log,
        "slave %s has unexpectedly disconnected - [%d] %s",
        m_id,
        ec.value(),
        ec.message()
    );

    std::for_each(
        m_sessions.begin(),
        m_sessions.end(),
        cancel_t(resource_error, "the session has been aborted")
    );

    m_sessions.clear();

    terminate();
}

void
slave_t::on_ping() {
    BOOST_ASSERT(m_state != states::dead);

    using namespace std::chrono;

    if(m_state == states::unknown) {
        auto uptime = duration_cast<duration<float>>(
#if defined(__clang__) || defined(HAVE_GCC47)
            steady_clock::now() - m_birthstamp
#else
            monotonic_clock::now() - m_birthstamp
#endif
        );

        COCAINE_LOG_DEBUG(
            m_log,
            "slave %s became active in %.03f seconds",
            m_id,
            uptime.count()
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

    m_channel->wr->write<rpc::heartbeat>(0UL);
}

void
slave_t::on_death(int code,
                  const std::string& reason)
{
    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s is committing suicide: %s",
        m_id,
        reason
    );

    m_engine.erase(m_id, code, reason);
}

void
slave_t::on_chunk(uint64_t session_id,
                  const std::string& chunk)
{
    BOOST_ASSERT(m_state == states::active);

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

    it->second->upstream->write(chunk.data(), chunk.size());
}

void
slave_t::on_error(uint64_t session_id,
                  int code_,
                  const std::string& reason)
{
    BOOST_ASSERT(m_state == states::active);

    error_code code = static_cast<error_code>(code_);

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
    BOOST_ASSERT(m_state == states::active);

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
slave_t::on_timeout(ev::timer&, int) {
    BOOST_ASSERT(m_state != states::dead);

    switch(m_state) {
        case states::unknown:
            COCAINE_LOG_WARNING(m_log, "slave %s has failed to activate", m_id);
            break;

        case states::active:
            COCAINE_LOG_WARNING(
                m_log,
                "slave %s has timed out, dropping %llu sessions",
                m_id,
                m_sessions.size()
            );

            std::for_each(
                m_sessions.begin(),
                m_sessions.end(),
                cancel_t(timeout_error, "the session had timed out")
            );

            m_sessions.clear();

            break;

        case states::inactive:
            COCAINE_LOG_WARNING(m_log, "slave %s has failed to deactivate", m_id);
            break;
    }

    terminate();
}

void
slave_t::on_idle(ev::timer&, int) {
    BOOST_ASSERT(m_state == states::active);
    BOOST_ASSERT(m_sessions.empty());

    COCAINE_LOG_DEBUG(m_log, "slave %s is idle, deactivating", m_id);

    m_channel->wr->write<rpc::terminate>(
        0UL,
        static_cast<int>(rpc::terminate::normal),
        std::string("idle")
    );

    m_state = states::inactive;
}

void
slave_t::terminate() {
    BOOST_ASSERT(m_state != states::dead);
    BOOST_ASSERT(m_sessions.empty());

    COCAINE_LOG_DEBUG(m_log, "slave %s has been terminated", m_id);

    m_heartbeat_timer.stop();
    m_idle_timer.stop();

    m_handle->terminate();
    m_handle.reset();

    // Closes our end of the socket.
    m_channel.reset();

    m_state = states::dead;
}

