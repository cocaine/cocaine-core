/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/service/node/slave.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/service/node/engine.hpp"
#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node/session.hpp"
#include "cocaine/detail/service/node/stream.hpp"

#include "cocaine/idl/rpc.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/rpc/actor.hpp"

#include "cocaine/traits/enum.hpp"
#include "cocaine/traits/literal.hpp"

#include <sstream>

#include <boost/circular_buffer.hpp>
#include <boost/lexical_cast.hpp>

#include <asio/posix/stream_descriptor.hpp>

#include <fcntl.h>
#include <unistd.h>

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;

struct slave_t::output_t  {
    std::array<char, 4096> buffer;
    boost::circular_buffer<std::string> lines;
    std::unique_ptr<api::handle_t> handler;
    asio::posix::stream_descriptor stream;

    output_t(unsigned long limit, std::unique_ptr<api::handle_t>&& handler, asio::io_service& loop) :
        lines(limit),
        handler(std::move(handler)),
        stream(loop, this->handler->stdout())
    {}

    ~output_t() {
        handler->terminate();
    }

    void cancel() {
        stream.cancel();
    }
};

slave_t::slave_t(const std::string& id,
                 const manifest_t& manifest,
                 const profile_t& profile,
                 context_t& context,
                 rebalance_type rebalance,
                 suicide_type suicide,
                 asio::io_service& asio) :
    m_context(context),
    m_log(context.log(manifest.name, {{ "uuid", id }})),
    m_asio(asio),
    m_manifest(manifest),
    m_profile(profile),
    m_id(id),
    m_rebalance(rebalance),
    m_suicide(suicide),
    m_state(states::unknown),
#ifdef COCAINE_HAS_FEATURE_STEADY_CLOCK
    m_birthstamp(std::chrono::steady_clock::now()),
#else
    m_birthstamp(std::chrono::monotonic_clock::now()),
#endif
    m_heartbeat_timer(asio),
    m_idle_timer(asio)
{
    asio.post(std::bind(&slave_t::activate, this));
}

slave_t::~slave_t() {
    COCAINE_LOG_DEBUG(m_log, "slave is terminating");
    BOOST_ASSERT(m_state == states::inactive);
    BOOST_ASSERT(m_sessions.empty() && m_queue.empty());
}

void
slave_t::bind(const std::shared_ptr<io::channel<protocol_type>>& channel) {
    BOOST_ASSERT(m_state == states::unknown);
    BOOST_ASSERT(!m_channel);

    m_channel = channel;
    m_channel->reader->read(m_message, std::bind(&slave_t::on_read, shared_from_this(), ph::_1));
}

void
slave_t::assign(const std::shared_ptr<session_t>& session) {
    m_asio.post(std::bind(&slave_t::do_assign, shared_from_this(), session));
}

void
slave_t::stop() {
    m_asio.post(std::bind(&slave_t::do_stop, shared_from_this()));
}

void
slave_t::do_stop() {
    BOOST_ASSERT(m_state == states::active);
    m_state = states::inactive;
    m_channel->writer->write(
        encoded<rpc::terminate>(1, rpc::terminate::normal, "the engine is shutting down"),
        std::bind(&slave_t::on_write, shared_from_this(), ph::_1)
    );
}

void
slave_t::do_assign(std::shared_ptr<session_t> session) {
    if(m_state == states::inactive) {
        COCAINE_LOG_WARNING(m_log, "unable to assign session: slave is inactive");
        try {
            session->upstream->error(error::resource_error, "slave is inactive");
            session->detach();
        } catch(const std::exception& err) {
            COCAINE_LOG_WARNING(m_log, "slave %s is unable to send error event to the upstream: %s", m_id, err.what());
        }
        return;
    }

    typedef api::policy_t::clock_type clock_type;
    if(session->event.policy.deadline > clock_type::time_point() && session->event.policy.deadline <= clock_type::now()) {
        COCAINE_LOG_DEBUG(m_log, "session %d has expired, dropping", session->id);
        session->upstream->error(error::deadline_error, "the session has expired in the queue");
        return;
    }
    m_idle_timer.cancel();

    if(m_sessions.size() >= m_profile.concurrency || m_state == states::unknown) {
        m_queue.push_back(session);
        return;
    }

    BOOST_ASSERT(m_state == states::active);
    m_sessions.insert(std::make_pair(session->id, session));

    COCAINE_LOG_DEBUG(m_log, "slave has started processing %d session", session->id);
    session->attach(m_channel->writer);
}

void
slave_t::activate() {
    COCAINE_LOG_DEBUG(m_log, "slave is activating, timeout: %.02f seconds",
        m_profile.startup_timeout
    );
    m_heartbeat_timer.expires_from_now(boost::posix_time::seconds(m_profile.startup_timeout));
    m_heartbeat_timer.async_wait(std::bind(&slave_t::on_timeout, shared_from_this(), ph::_1));

    COCAINE_LOG_DEBUG(m_log, "slave is spawning using '%s'", m_manifest.executable);

    auto isolate = m_context.get<api::isolate_t>(
        m_profile.isolate.type,
        m_context,
        m_manifest.name,
        m_profile.isolate.args
    );

    // Prepare command line arguments for worker instance.
    api::string_map_t args;
    auto& locator = m_context.locate("locator").get();
    args["--uuid"]     = m_id;
    args["--app"]      = m_manifest.name;
    args["--endpoint"] = m_manifest.endpoint;
    args["--locator"]  = cocaine::format("%s:%d", m_context.config.network.hostname, locator.endpoints().front().port());

    // Spawn a worker instance and start reading standard outputs of it.
    try {
        m_output = std::make_unique<output_t>(
            m_profile.crashlog_limit,
            isolate->spawn(m_manifest.executable, args, m_manifest.environment),
            m_asio
        );
        m_output->stream.async_read_some(
            asio::buffer(m_output->buffer.data(), m_output->buffer.size()),
            std::bind(&slave_t::on_output, shared_from_this(), ph::_1, ph::_2, std::string())
        );
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to spawn more slaves: [%d] %s", e.code().value(), e.code().message());
        m_suicide(m_id, e.code().value(), e.code().message());
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(m_log, "unable to spawn more slaves: %s", e.what());
        m_suicide(m_id, -1, e.what());
    } catch(...) {
        COCAINE_LOG_ERROR(m_log, "unable to spawn more slaves: unknown exception");
        m_suicide(m_id, -1, "unknown");
    }
}

void
slave_t::on_read(const std::error_code& ec) {
    if(ec) {
        if(ec == asio::error::operation_aborted) {
            return;
        }
        on_failure(ec);
    } else {
        on_message(m_message);

        if(m_state != states::inactive) {
            m_channel->reader->read(m_message, std::bind(&slave_t::on_read, shared_from_this(), ph::_1));
        }
    }
}

void
slave_t::on_write(const std::error_code& ec) {
    if(ec) {
        if(ec == asio::error::operation_aborted) {
            return;
        }
        on_failure(ec);
    }
}

void
slave_t::on_output(const std::error_code& ec, std::size_t size, std::string left) {
    if(ec) {
        if(ec == asio::error::operation_aborted) {
            return;
        }

        COCAINE_LOG_DEBUG(m_log, "slave has failed to read output: %s", ec.message());
        return;
    }

    BOOST_ASSERT(m_output);

    COCAINE_LOG_DEBUG(m_log, "slave received %d bytes of output", size);
    std::stringstream stream(left);
    stream << std::string(m_output->buffer.data(), size);

    std::string line;
    while(std::getline(stream, line)) {
        m_output->lines.push_back(line);

        if(m_profile.log_output) {
            COCAINE_LOG_DEBUG(m_log, "slave output: %s", line);
        }
    }

    m_output->stream.async_read_some(
        asio::buffer(m_output->buffer.data(), m_output->buffer.size()),
        std::bind(&slave_t::on_output, shared_from_this(), ph::_1, ph::_2, line)
    );
}

void
slave_t::on_message(const io::decoder_t::message_type& message) {
    COCAINE_LOG_DEBUG(m_log, "slave received type %d message in session %d", message.type(), message.span());

    try {
        process(message);
    } catch (const std::bad_cast&) {
        COCAINE_LOG_WARNING(m_log, "slave dropped unknown message in session %d: message is corrupted", message.span());
        terminate(rpc::terminate::code::abnormal, "slave has detected session corruption");
    }
}

void
slave_t::process(const io::decoder_t::message_type& message) {
    switch(message.type()) {
    case event_traits<rpc::heartbeat>::id:
        on_ping();
        break;
    case event_traits<rpc::terminate>::id: {
        rpc::terminate::code code;
        std::string reason;
        io::type_traits<
            typename io::event_traits<rpc::terminate>::argument_type
        >::unpack(message.args(), code, reason);
        on_death(code, reason);
        break;
    }
    case event_traits<rpc::chunk>::id: {
        std::string chunk;
        io::type_traits<
            typename io::event_traits<rpc::chunk>::argument_type
        >::unpack(message.args(), chunk);
        on_chunk(message.span(), chunk);
        break;
    }
    case event_traits<rpc::error>::id: {
        int code;
        std::string reason;
        io::type_traits<
            typename io::event_traits<rpc::error>::argument_type
        >::unpack(message.args(), code, reason);
        on_error(message.span(), code, reason);
        break;
    }
    case event_traits<rpc::choke>::id:
        on_choke(message.span());
        break;
    default:
        COCAINE_LOG_WARNING(m_log, "slave dropped unknown type %d message in session %d", message.type(), message.span());
    }
}

void
slave_t::on_failure(const std::error_code& ec) {
    if(ec) {
        if(ec == asio::error::operation_aborted) {
            return;
        }

        switch(m_state) {
        case states::unknown:
        case states::active:
            COCAINE_LOG_ERROR(m_log, "slave has unexpectedly disconnected: [%d] %s", ec.value(), ec.message());
            dump();
            terminate(rpc::terminate::code::normal, "slave has unexpectedly disconnected");
            break;
        case states::inactive:
            terminate(rpc::terminate::code::normal, "slave has shut itself down");
            break;
        default:
            BOOST_ASSERT(false);
        }
    }
}

void
slave_t::on_ping() {
    if(m_state == states::inactive) {
        // Slave is already inactive, do nothing.
        return;
    }

    COCAINE_LOG_DEBUG(m_log, "slave is resetting heartbeat timeout to %.02f seconds", m_profile.heartbeat_timeout);

    m_heartbeat_timer.expires_from_now(boost::posix_time::seconds(m_profile.heartbeat_timeout));
    m_heartbeat_timer.async_wait(std::bind(&slave_t::on_timeout, shared_from_this(), ph::_1));
    m_channel->writer->write(
        encoded<rpc::heartbeat>(1),
        std::bind(&slave_t::on_write, shared_from_this(), ph::_1)
    );

    if(m_state == states::unknown) {
#ifdef COCAINE_HAS_FEATURE_STEADY_CLOCK
        auto now = std::chrono::steady_clock::now();
#else
        auto now = std::chrono::monotonic_clock::now();
#endif
        const auto uptime = std::chrono::duration_cast<
            std::chrono::duration<float>
        >(now - m_birthstamp);

        COCAINE_LOG_DEBUG(m_log, "slave became active in %.03f seconds", uptime.count());

        m_state = states::active;

        if(m_profile.idle_timeout) {
            // Start the idle timer, which will kill the slave when it's not used.
            m_idle_timer.expires_from_now(boost::posix_time::seconds(m_profile.idle_timeout));
            m_idle_timer.async_wait(std::bind(&slave_t::on_idle, shared_from_this(), ph::_1));
        }

        pump();
    }
}

void
slave_t::on_death(int code, const std::string& reason) {
    COCAINE_LOG_DEBUG(m_log, "slave is committing suicide: %s", reason);

    // NOTE: This is the only case where code could be abnormal, triggering
    // the engine shutdown. Socket errors are not considered abnormal.
    terminate(code, reason);
}

void
slave_t::on_chunk(uint64_t session_id, const std::string& chunk) {
    BOOST_ASSERT(m_state == states::active);

    COCAINE_LOG_DEBUG(m_log, "slave received chunk in session %d", session_id)(
        "size", chunk.size()
    );

    auto it = m_sessions.find(session_id);
    if(it == m_sessions.end()) {
        COCAINE_LOG_WARNING(m_log, "slave received orphan session %d chunk", session_id);
        return;
    }

    try {
        it->second->upstream->write(chunk.data(), chunk.size());
    } catch (const cocaine::error_t& err) {
        COCAINE_LOG_WARNING(m_log, "slave is unable to send write event to the upstream: %s", err.what());
    }
}

void
slave_t::on_error(uint64_t session_id, int code, const std::string& reason) {
    BOOST_ASSERT(m_state == states::active);

    COCAINE_LOG_DEBUG(m_log, "slave received error in session %d", session_id)(
        "errno", code,
        "reason", reason
    );

    auto it = m_sessions.find(session_id);
    if(it == m_sessions.end()) {
        COCAINE_LOG_WARNING(m_log, "slave received orphan session %d error", session_id);
        return;
    }

    try {
        it->second->upstream->error(code, reason);
    } catch (const cocaine::error_t& err) {
        COCAINE_LOG_WARNING(m_log, "slave is unable to send error event to the upstream: %s", err.what());
    }
}

void
slave_t::on_choke(uint64_t session_id) {
    BOOST_ASSERT(m_state == states::active);

    COCAINE_LOG_DEBUG(m_log, "slave has completed session %d", session_id);

    auto it = m_sessions.find(session_id);
    if(it == m_sessions.end()) {
        COCAINE_LOG_WARNING(m_log, "slave received orphan session %d choke", session_id);
        return;
    }

    auto session = std::move(it->second);
    m_sessions.erase(it);

    try {
        session->upstream->close();
        session->detach();
    } catch (const cocaine::error_t& err) {
        COCAINE_LOG_WARNING(m_log, "slave is unable to send close event to the upstream: %s", err.what());
    }

    // Destroy the session before calling the potentially heavy queue pumps.
    session.reset();

    pump();
}

void
slave_t::on_timeout(const std::error_code& ec) {
    if(ec == asio::error::operation_aborted) {
        return;
    }

    // Timer has expired.
    switch(m_state) {
    case states::unknown:
        COCAINE_LOG_ERROR(m_log, "slave has failed to activate");
        break;
    case states::active:
        COCAINE_LOG_ERROR(m_log, "slave has timed out");
        break;
    case states::inactive:
        COCAINE_LOG_ERROR(m_log, "slave has failed to deactivate");
        break;
    }

    dump();
    terminate(rpc::terminate::code::normal, "slave has timed out");
}

void
slave_t::on_idle(const std::error_code& ec) {
    if(ec == asio::error::operation_aborted) {
        return;
    }

    BOOST_ASSERT(m_state == states::active);
    BOOST_ASSERT(m_sessions.empty() && m_queue.empty());

    COCAINE_LOG_DEBUG(m_log, "slave is idle, deactivating");
    m_state = states::inactive;

    m_channel->writer->write(
        encoded<rpc::terminate>(1, rpc::terminate::normal, "slave is idle"),
        std::bind(&slave_t::on_write, shared_from_this(), ph::_1)
    );
}

void
slave_t::pump() {
    session_queue_t::value_type session;

    while(!m_queue.empty()) {
        if(m_queue.empty() || m_sessions.size() >= m_profile.concurrency) {
            break;
        }

        // Move out a new session from the queue.
        session = std::move(m_queue.front());

        // Destroy an empty session husk.
        m_queue.pop_front();

        assign(session);
    }

    if(m_sessions.empty() && m_profile.idle_timeout) {
        m_idle_timer.expires_from_now(boost::posix_time::seconds(m_profile.idle_timeout));
        m_idle_timer.async_wait(std::bind(&slave_t::on_idle, shared_from_this(), ph::_1));
    }

    m_rebalance();
}

void
slave_t::dump() {
    if(!m_output) {
        COCAINE_LOG_WARNING(m_log, "No output from slave - slave failed to create handle");
        return;
    }

    std::vector<std::string> dump;
    std::copy(m_output->lines.begin(), m_output->lines.end(), std::back_inserter(dump));

    if(dump.empty()) {
        COCAINE_LOG_WARNING(m_log, "slave has died in silence");
        return;
    }

    const auto now = std::chrono::duration_cast<
        std::chrono::microseconds
    >(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto key = cocaine::format("%lld:%s", now, m_id);

    COCAINE_LOG_INFO(m_log, "slave is dumping output to 'crashlogs/%s'", key);

    try {
        api::storage(m_context, "core")->put("crashlogs", key, dump, std::vector<std::string> {
            m_manifest.name
        });
    } catch(const storage_error_t& err) {
        COCAINE_LOG_ERROR(m_log, "slave is unable to save the crashlog: %s", err.what());
    }
}

void
slave_t::terminate(int code, const std::string& reason) {
    COCAINE_LOG_DEBUG(m_log, "terminating slave: [%d] %d", code, reason);
    m_state = states::inactive;

    if(!m_sessions.empty()) {
        COCAINE_LOG_WARNING(m_log, "slave dropping %d sessions", m_sessions.size());
        for(auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            try {
                it->second->upstream->error(error::resource_error, reason);
                it->second->detach();
            } catch (const std::exception& err) {
                COCAINE_LOG_WARNING(m_log, "slave is unable to send error event to the upstream: %s", err.what());
            }
        }

        m_sessions.clear();
    }

    m_heartbeat_timer.cancel();
    m_idle_timer.cancel();

    // Closes our end of the socket.
    m_channel.reset();

    // Cancel fetching output. I don't know what is better for now:
    // * to cancel output stream - and to lose everything from stdout;
    // * or to do nothing and possibly wait forever.
    // also there is a possibility we could not initialize m_output till this time (f.e. on spawn error)
    if(m_output) {
        m_output->cancel();
    }

    COCAINE_LOG_DEBUG(m_log, "slave has cancelled its handlers");
    m_suicide(m_id, code, reason);
}
