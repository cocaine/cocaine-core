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

namespace cocaine { namespace engine {

class slave_t:
    public boost::noncopyable
{
    public:
        enum states: int {
            unknown,
            idle,
            busy,
            dead
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
        process(const io::message<rpc::ping>& message);

        void
        process(const io::message<rpc::chunk>& message);

        void
        process(const io::message<rpc::error>& message);

        void
        process(const io::message<rpc::choke>& message);

        template<class Event>
        void
        send(const io::message<Event>& message);

    public:
        states
        state() const {
            return m_state;
        }

        const unique_id_t&
        id() const {
            return m_id;
        }

    private:
        void
        on_timeout(ev::timer&, int);

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
        ev::timer m_heartbeat_timer;
    
        // Current slave state.
        states m_state;

        // Slave ID.
        const unique_id_t m_id;

        // Actual slave handle.    
        std::unique_ptr<api::handle_t> m_handle;

        // Current job.
        boost::shared_ptr<session_t> m_session;
};

template<class Event>
void
slave_t::send(const io::message<Event>& message) {
    if(m_state == states::dead) {
        throw cocaine::error_t("the slave is dead");
    }

    m_engine->send(
        m_id,
        message
    );
}

}} // namespace cocaine::engine

#endif
