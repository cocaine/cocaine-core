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
#include "cocaine/asio/reactor.hpp"
#include "cocaine/detail/unique_id.hpp"

#include <chrono>

#include <boost/circular_buffer.hpp>

namespace cocaine { namespace engine {

using io::reactor_t;

struct pipe_t {
    typedef int endpoint_type;

    pipe_t(endpoint_type endpoint);
   ~pipe_t();

    int
    fd() const;

    ssize_t
    read(char* buffer, size_t size, std::error_code& ec);

private:
    int m_pipe;
};

struct session_t;

class slave_t {
    COCAINE_DECLARE_NONCOPYABLE(slave_t)

    public:
        enum class states: int {
            unknown,
            active,
            inactive,
            dead
        };

    public:
        slave_t(context_t& context,
                reactor_t& reactor,
                const manifest_t& manifest,
                const profile_t& profile,
                engine_t& engine);

       ~slave_t();

        // Binding

        void
        bind(const std::shared_ptr<io::channel<io::socket<io::local>>>& channel);

        // Sessions

        void
        assign(std::shared_ptr<session_t>&& session);

        // Termination

        void
        stop();

    public:
        unique_id_t
        id() const {
            return m_id;
        }

        states
        state() const {
            return m_state;
        }

        size_t
        load() const {
            return m_sessions.size();
        }

    private:
        void
        on_message(const io::message_t& message);

        void
        on_disconnect(const std::error_code& ec);

        // RPC

        void
        on_ping();

        void
        on_death(int code,
                 const std::string& reason);

        void
        on_chunk(uint64_t session_id,
                 const std::string& chunk);

        void
        on_error(uint64_t session_id,
                 error_code code,
                 const std::string& reason);

        void
        on_choke(uint64_t session_id);

        // Health

        void
        on_timeout(ev::timer&, int);

        void
        on_idle(ev::timer&, int);

        ssize_t
        on_output(const char* data, size_t size);

        // Housekeeping

        void
        dump();

        void
        terminate();

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        // Configuration

        const manifest_t& m_manifest;
        const profile_t& m_profile;

        // Controlling engine

        engine_t& m_engine;

        // Slave ID

        const unique_id_t m_id;

        // Slave health monitoring

        states m_state;

#if defined(__clang__) || defined(HAVE_GCC47)
        const std::chrono::steady_clock::time_point m_birthstamp;
#else
        const std::chrono::monotonic_clock::time_point m_birthstamp;
#endif

        ev::timer m_heartbeat_timer;
        ev::timer m_idle_timer;

        // Slave handling

        std::unique_ptr<api::handle_t> m_handle;

        // Slave output capture

        std::unique_ptr<io::readable_stream<pipe_t>> m_pipe;
        boost::circular_buffer<std::string> m_output_ring;

        // I/O

        std::shared_ptr<io::channel<io::socket<io::local>>> m_channel;

        // Active sessions

        typedef std::map<
            uint64_t,
            std::shared_ptr<session_t>
        > session_map_t;

        session_map_t m_sessions;
};

}} // namespace cocaine::engine

#endif
