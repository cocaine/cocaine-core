/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include <fcntl.h>
#include <unistd.h>

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;

using namespace std::placeholders;

pipe_t::pipe_t(endpoint_type endpoint):
    m_pipe(endpoint)
{ }

pipe_t::~pipe_t() {
    ::close(m_pipe);
}

int
pipe_t::fd() const {
    return m_pipe;
}

ssize_t
pipe_t::read(char* buffer, size_t size, std::error_code& ec) {
    ssize_t length = ::read(m_pipe, buffer, size);

    if(length == -1) {
        switch(errno) {
            case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
            case EWOULDBLOCK:
#endif
            case EINTR:
                break;

            default:
                ec = std::error_code(errno, std::system_category());
        }
    }

    return length;
}

namespace {
    struct ignore {
        void
        operator()(const std::error_code& /* ec */) {
            // Do nothing.
        }
    };
}

slave_t::slave_t(context_t& context,
                 reactor_t& reactor,
                 const manifest_t& manifest,
                 const profile_t& profile,
                 engine_t& engine):
    m_context(context),
    m_log(new logging::log_t(context, cocaine::format("app/%s", manifest.name))),
    m_reactor(reactor),
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
    m_idle_timer(reactor.native()),
    m_output_ring(50)
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

    // Standard output capture

    std::array<int, 2> pipes;

    if(::pipe(pipes.data()) != 0) {
        throw std::system_error(errno, std::system_category(), "unable to create an output pipe");
    }

    // Our end.
    ::fcntl(pipes[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(pipes[0], F_SETFL, O_NONBLOCK);

    // Slave's end.
    ::fcntl(pipes[1], F_SETFD, FD_CLOEXEC);

    COCAINE_LOG_DEBUG(m_log, "slave %s spawning '%s'", m_id, m_manifest.slave);

    try {
        m_handle = isolate->spawn(m_manifest.slave, args, environment, pipes[1]);
    } catch(...) {
        ::close(pipes[0]);
        ::close(pipes[1]);

        throw;
    }

    m_pipe.reset(new readable_stream<pipe_t>(reactor, pipes[0]));

    m_pipe->bind(
        std::bind(&slave_t::on_output, this, _1, _2),
        ignore()
    );

    ::close(pipes[1]);
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
    m_channel->wr->write<rpc::terminate>(0UL, rpc::terminate::normal, "shutdown");
}

void
slave_t::on_message(const message_t& message) {
    COCAINE_LOG_DEBUG(
        m_log,
        "received type %d message in session %d from slave %s",
        message.id(),
        message.band(),
        m_id
    );

    switch(message.id()) {
        case event_traits<rpc::heartbeat>::id:
            on_ping();
            break;

        case event_traits<rpc::terminate>::id: {
            rpc::terminate::code code;
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
            error_code code;
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
                "dropping unknown type %d message in session %d from slave %s",
                message.id(),
                message.band(),
                m_id
            );
    }
}

namespace {
    struct detach_with {
        template<class T>
        void
        operator()(const T& session) const {
            session.second->upstream->error(code, message);
            session.second->detach();
        }

        const error_code code;
        const std::string message;
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

    m_state = states::inactive;

    std::for_each(m_sessions.begin(), m_sessions.end(), detach_with {
        resource_error,
        "the session has been aborted"
    });

    m_sessions.clear();

    dump();
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
slave_t::on_death(int code, const std::string& reason) {
    COCAINE_LOG_DEBUG(
        m_log,
        "slave %s is committing suicide: %s",
        m_id,
        reason
    );

    m_state = states::inactive;

    std::for_each(m_sessions.begin(), m_sessions.end(), detach_with {
        resource_error,
        "the session has been aborted"
    });

    m_sessions.clear();

    m_reactor.post(
        std::bind(&engine_t::erase, std::ref(m_engine), m_id, code, reason)
    );
}

void
slave_t::on_chunk(uint64_t session_id, const std::string& chunk) {
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
slave_t::on_error(uint64_t session_id, error_code code, const std::string& reason) {
    BOOST_ASSERT(m_state == states::active);

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

            m_state = states::inactive;

            std::for_each(m_sessions.begin(), m_sessions.end(), detach_with {
                timeout_error,
                "the session had timed out"
            });

            m_sessions.clear();

            break;

        case states::inactive:
            COCAINE_LOG_WARNING(m_log, "slave %s has failed to deactivate", m_id);
            break;

        case states::dead:
            COCAINE_LOG_WARNING(m_log, "slave %s is being overkilled", m_id);
            break;
    }

    dump();
    terminate();
}

void
slave_t::on_idle(ev::timer&, int) {
    BOOST_ASSERT(m_state == states::active);
    BOOST_ASSERT(m_sessions.empty());

    COCAINE_LOG_DEBUG(m_log, "slave %s is idle, deactivating", m_id);

    m_channel->wr->write<rpc::terminate>(0UL, rpc::terminate::normal, "idle");

    m_state = states::inactive;
}

ssize_t
slave_t::on_output(const char* data, size_t size) {
    std::string input(data, size),
                line;

    std::istringstream stream(input);

    while(stream) {
        std::getline(stream, line);

        if(stream.eof()) {
            return stream.tellg() - static_cast<std::streamoff>(line.size());
        }

        m_output_ring.push_back(line);
    }

    return size;
}

void
slave_t::dump() {
    if(m_output_ring.empty()) {
        COCAINE_LOG_INFO(m_log, "slave %s died in silence", m_id);
        return;
    }

    std::string key = cocaine::format("%s:%s", m_manifest.name, m_id);

    COCAINE_LOG_INFO(m_log, "slave %s dumping output to 'crashlogs/%s'", m_id, key);

    std::vector<std::string> dump;
    std::copy(m_output_ring.begin(), m_output_ring.end(), std::back_inserter(dump));

    api::storage(m_context, "core")->put("crashlogs", key, dump);
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

