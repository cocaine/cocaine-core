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

#ifndef COCAINE_SLAVE_HPP
#define COCAINE_SLAVE_HPP

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/messaging.hpp"
#include "cocaine/unique_id.hpp"

#include "cocaine/api/isolate.hpp"

namespace cocaine { namespace engine {

class slave_t:
    public boost::noncopyable
{
    public:
        enum class state_t: int {
            unknown,
            active,
            inactive,
            dead
        };

    public:
        slave_t(context_t& context,
                const manifest_t& manifest,
                const profile_t& profile,
                engine_t& engine);

        ~slave_t();

        // Sessions

        void
        assign(boost::shared_ptr<session_t>&& session);
       
        // Slave RPC

        void
        on_ping();

        void
        on_chunk(uint64_t session_id,
                 const std::string& chunk);

        void
        on_error(uint64_t session_id,
                 error_code code,
                 const std::string& reason);

        void
        on_choke(uint64_t session_id);

        // Slave I/O

        void
        send(const std::string& blob);

        void
        send(const std::vector<std::string>& blobs);

    public:
        unique_id_t
        id() const {
            return m_id;
        }

        state_t
        state() const {
            return m_state;
        }

        size_t
        load() const {
            return m_sessions.size();
        }

    private:
        void
        on_timeout(ev::timer&, int);

        void
        on_idle(ev::timer&, int);

        void
        terminate();

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        const manifest_t& m_manifest;
        const profile_t& m_profile;

        // Controlling engine.
        engine_t& m_engine;

        // Slave ID.
        const unique_id_t m_id;

        // Current slave state.
        state_t m_state;

        // Slave health monitoring.
        ev::timer m_heartbeat_timer;
        ev::timer m_idle_timer;
    
        // Actual slave process handle.    
        std::unique_ptr<api::handle_t> m_handle;

        typedef std::map<
            uint64_t,
            boost::shared_ptr<session_t>
        > session_map_t;

        // Current sessions.
        session_map_t m_sessions;
};

}} // namespace cocaine::engine

#endif
