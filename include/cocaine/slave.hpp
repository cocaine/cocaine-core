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
#include "cocaine/engine.hpp"
#include "cocaine/rpc.hpp"
#include "cocaine/unique_id.hpp"

#include "cocaine/helpers/atomic.hpp"

namespace cocaine { namespace engine {

class slave_t:
    public boost::noncopyable
{
    public:
        struct states {
            enum value: int {
                unknown,
                active,
                inactive,
                dead
            };
        };

    public:
        slave_t(context_t& context,
                const manifest_t& manifest,
                const profile_t& profile,
                engine_t * const engine);

        ~slave_t();

        void
        assign(const boost::shared_ptr<session_t>& session);
       
        void
        on_ping();

        void
        on_chunk(const unique_id_t& session_id,
                 const std::string& message);

        void
        on_error(const unique_id_t& session_id,
                 error_code code,
                 const std::string& message);

        void
        on_choke(const unique_id_t& session_id);

        template<class Event, typename... Args>
        bool
        send(Args&&... args);

        bool
        send(int message_id,
             const std::string& message)
        {
            BOOST_ASSERT(m_state == states::active);

            return m_engine->send(
                m_id,
                message_id,
                message
            );
        }

    public:
        const unique_id_t&
        id() const {
            return m_id;
        }

        states::value
        state() const {
            return static_cast<states::value>(m_state.load());
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
        rearm();

        void
        terminate();
 
    private:
        context_t& m_context; 
        boost::shared_ptr<logging::logger_t> m_log;

        const manifest_t& m_manifest;
        const profile_t& m_profile;

        // Controlling engine.
        engine_t * const m_engine;

        // Slave health monitoring.
        ev::timer m_heartbeat_timer,
                  m_idle_timer;
    
        // Slave ID.
        const unique_id_t m_id;

        // Current slave state.
        std::atomic<int> m_state;

        // Actual slave process handle.    
        std::unique_ptr<api::handle_t> m_handle;

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            unique_id_t,
            boost::shared_ptr<session_t>
        > session_map_t;

        // Current sessions.
        session_map_t m_sessions;
};

template<class Event, typename... Args>
bool
slave_t::send(Args&&... args) {
    BOOST_ASSERT(m_state == states::active);

    return m_engine->send<Event>(
        m_id,
        std::forward<Args>(args)...
    );
}

}} // namespace cocaine::engine

#endif
