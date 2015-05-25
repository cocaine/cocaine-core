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

#ifndef COCAINE_ENGINE_SLAVE_HPP
#define COCAINE_ENGINE_SLAVE_HPP

#include "cocaine/common.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/detail/service/node/forwards.hpp"
#include "cocaine/detail/service/node/queue.hpp"

#include "cocaine/rpc/asio/channel.hpp"
#include "cocaine/rpc/asio/decoder.hpp"

#include <atomic>
#include <chrono>

#include <asio/local/stream_protocol.hpp>

namespace cocaine { namespace engine {

struct session_t;

class slave_t : public std::enable_shared_from_this<slave_t> {
    COCAINE_DECLARE_NONCOPYABLE(slave_t)

    typedef asio::local::stream_protocol protocol_type;

    enum class states {
        unknown,
        active,
        inactive
    };

    context_t& m_context;

    const std::unique_ptr<logging::log_t> m_log;

    // IO.
    asio::io_service& m_asio;

    // Configuration

    const manifest_t& m_manifest;
    const profile_t& m_profile;

    // Slave ID.
    const std::string m_id;

    // Self engine-control.
    typedef std::function<void()> rebalance_type;
    typedef std::function<void(const std::string&, int, const std::string&)> suicide_type;
    rebalance_type m_rebalance;
    suicide_type m_suicide;

    // Health.
    states m_state;

#ifdef COCAINE_HAS_FEATURE_STEADY_CLOCK
    const std::chrono::steady_clock::time_point m_birthstamp;
#else
    const std::chrono::monotonic_clock::time_point m_birthstamp;
#endif

    asio::deadline_timer m_heartbeat_timer;
    asio::deadline_timer m_idle_timer;

    // IO communication with worker.
    io::decoder_t::message_type m_message;
    std::shared_ptr<io::channel<protocol_type>> m_channel;

    // Active sessions (or channels now?).
    typedef std::map<
        uint64_t,
        std::shared_ptr<session_t>
    > session_map_t;
    session_map_t m_sessions;

    // Tagged session queue.
    session_queue_t m_queue;

    // Output capture.
    struct output_t;
    std::unique_ptr<output_t> m_output;

public:
    slave_t(const std::string& id,
            const manifest_t& manifest,
            const profile_t& profile,
            context_t& context,
            rebalance_type rebalance,
            suicide_type suicide,
            asio::io_service& asio);
   ~slave_t();

    // Bind IO channel. Single shot.
    void
    bind(const std::shared_ptr<io::channel<protocol_type>>& channel);

    // Session scheduling.
    void
    assign(const std::shared_ptr<session_t>& session);

    // Terminate the slave by sending terminate message to the worker instance.
    void
    stop();

public:
    bool
    active() const {
        return m_state == states::active;
    }

    size_t
    load() const {
        return m_sessions.size();
    }

    // Prepare all timers and spawn a worker instance.
    void
    activate();

private:
    void
    do_assign(std::shared_ptr<session_t> session);

    void
    do_stop();

    // Called on any read event from the worker.
    void
    on_read(const std::error_code& ec);

    // Called on any write event to the worker.
    void
    on_write(const std::error_code& ec);

    // Called on any read event from worker's outputs.
    void
    on_output(const std::error_code& ec, std::size_t size, std::string left);

    // Called by read callback on every successful message received from a worker.
    void
    on_message(const io::decoder_t::message_type& m_message);

    void
    process(const io::decoder_t::message_type& message);

    // On any socket error associated with worker.
    void
    on_failure(const std::error_code& ec);

    // Heartbeat handler.
    void
    on_ping();

    // Terminate handler.
    void
    on_death(int code, const std::string& reason);

    // Chunk handler.
    void
    on_chunk(uint64_t session_id, const std::string& chunk);

    // Error handler.
    void
    on_error(uint64_t session_id, int code, const std::string& reason);

    // Choke handler.
    void
    on_choke(uint64_t session_id);

    // Called on heartbeat timeout.
    void
    on_timeout(const std::error_code& ec);

    // Called on idle timeout.
    void
    on_idle(const std::error_code& ec);

    // Housekeeping.
    void
    pump();

    void
    dump();

    void
    terminate(int code, const std::string& reason);
};

}} // namespace cocaine::engine

#endif
