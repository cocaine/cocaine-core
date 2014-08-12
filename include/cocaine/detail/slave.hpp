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

#ifndef COCAINE_ENGINE_SLAVE_HPP
#define COCAINE_ENGINE_SLAVE_HPP

#include "cocaine/common.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/detail/atomic.hpp"
#include "cocaine/detail/queue.hpp"

#include <chrono>

#include <boost/circular_buffer.hpp>

namespace ev {
    struct timer;
}

namespace cocaine { namespace engine {

struct session_t;

class slave_t {
    COCAINE_DECLARE_NONCOPYABLE(slave_t)

    enum class states {
        unknown,
        active,
        inactive
    };

    public:
        slave_t(context_t& context,
                io::reactor_t& reactor,
                const manifest_t& manifest,
                const profile_t& profile,
                const std::string& id,
                engine_t& engine);

       ~slave_t();

        // I/O

        void
        bind(const std::shared_ptr<io::channel<io::socket<io::local>>>& channel);

        // Session scheduling

        void
        assign(const std::shared_ptr<session_t>& session);

        // Termination

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

    private:
        void
        on_message(const io::message_t& message);

        void
        on_failure(const std::error_code& ec);

        // Streaming RPC

        void
        on_ping();

        void
        on_death(int code, const std::string& reason);

        void
        on_chunk(uint64_t session_id, const std::string& chunk);

        void
        on_error(uint64_t session_id, int code, const std::string& reason);

        void
        on_choke(uint64_t session_id);

        // Health

        void
        on_timeout(ev::timer&, int);

        void
        on_idle(ev::timer&, int);

        size_t
        on_output(const char* data, size_t size);

        // Housekeeping

        void
        pump();

        void
        dump();

        void
        terminate(int code, const std::string& reason);

    private:
        context_t& m_context;

        const std::unique_ptr<logging::log_t> m_log;

        // I/O Reactor

        io::reactor_t& m_reactor;

        // Configuration

        const manifest_t& m_manifest;
        const profile_t& m_profile;

        // Slave ID

        const std::string m_id;

        // Controlling engine

        engine_t& m_engine;

        // Health

        states m_state;

#if defined(COCAINE_HAVE_FEATURE_STEADY_CLOCK)
        const std::chrono::steady_clock::time_point m_birthstamp;
#else
        const std::chrono::monotonic_clock::time_point m_birthstamp;
#endif

        std::unique_ptr<ev::timer> m_heartbeat_timer;
        std::unique_ptr<ev::timer> m_idle_timer;

        // Native handle

        std::unique_ptr<api::handle_t> m_handle;

        // Output capture

        struct pipe_t;

        std::unique_ptr<io::readable_stream<pipe_t>> m_output_pipe;
        boost::circular_buffer<std::string> m_output_ring;

        // I/O channel

        std::shared_ptr<io::channel<io::socket<io::local>>> m_channel;

        // Active sessions

        typedef std::map<
            uint64_t,
            std::shared_ptr<session_t>
        > session_map_t;

        session_map_t m_sessions;

        // Tagged session queue

        session_queue_t m_queue;

        // Slave interlocking

        std::mutex m_mutex;
};

}} // namespace cocaine::engine

#endif
