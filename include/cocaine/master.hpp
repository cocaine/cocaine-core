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

#ifndef COCAINE_MASTER_HPP
#define COCAINE_MASTER_HPP

#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/transition.hpp>

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/events.hpp"
#include "cocaine/unique_id.hpp"

#include "cocaine/api/isolate.hpp"

namespace cocaine { namespace engine {

namespace sc = boost::statechart;

// Slave states

namespace slave {
    struct unknown;
    struct alive;
        struct idle;
        struct busy;
    struct dead;
}

// Master FSM

class master_t:
    public sc::state_machine<master_t, slave::unknown>
{
    friend struct slave::unknown;
    friend struct slave::alive;
    friend struct slave::busy;

    public:
        master_t(context_t& context,
                 ev::loop_ref& loop,
                 const manifest_t& manifest,
                 const profile_t& profile);

        ~master_t();
       
        const unique_id_t&
        id() const;

        bool operator==(const master_t& other) const;

    private:
        void on_initialize(const events::heartbeat& event);
        void on_heartbeat(const events::heartbeat& event);
        void on_terminate(const events::terminate& event);
        void on_timeout(ev::timer&, int);

    private:
        context_t& m_context; 
        boost::shared_ptr<logging::logger_t> m_log;

        ev::loop_ref& m_loop;

        const manifest_t& m_manifest;
        const profile_t& m_profile;

        // Host-unique identifier for this slave.
        const unique_id_t m_id;

        ev::timer m_heartbeat_timer;
    
        // The actual slave handle.    
        std::unique_ptr<api::handle_t> m_handle;
};

namespace slave {
    struct unknown:
        public sc::simple_state<unknown, master_t> 
    {
        typedef boost::mpl::list<
            sc::transition<events::heartbeat, alive, master_t, &master_t::on_initialize>,
            sc::transition<events::terminate, dead, master_t, &master_t::on_terminate>
        > reactions;
    };

    struct alive:
        public sc::simple_state<alive, master_t, idle>
    {
        ~alive();

        void on_invoke(const events::invoke& event);
        void on_choke(const events::choke& event);

        typedef boost::mpl::list<
            sc::in_state_reaction<events::heartbeat, master_t, &master_t::on_heartbeat>,
            sc::transition<events::terminate, dead, master_t, &master_t::on_terminate>
        > reactions;

        boost::shared_ptr<job_t> job;
    };

    struct idle: 
        public sc::simple_state<idle, alive>
    {
        typedef boost::mpl::list<
            sc::transition<events::invoke, busy, alive, &alive::on_invoke>
        > reactions;
    };

    struct busy:
        public sc::simple_state<busy, alive>
    {
        void on_chunk(const events::chunk& event);
        void on_error(const events::error& event);

        typedef boost::mpl::list<
            sc::in_state_reaction<events::chunk, busy, &busy::on_chunk>,
            sc::in_state_reaction<events::error, busy, &busy::on_error>,
            sc::transition<events::choke, idle, alive, &alive::on_choke>
        > reactions;

        const boost::shared_ptr<job_t>& job() const {
            return context<alive>().job;
        }
    };

    struct dead:
        public sc::simple_state<dead, master_t>
    { };
}

}} // namespace cocaine::engine

#endif
