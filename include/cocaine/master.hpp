//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef COCAINE_MASTER_HPP
#define COCAINE_MASTER_HPP

#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/transition.hpp>

#include "cocaine/common.hpp"

// Has to be included after common.h
#include <ev++.h>

#include "cocaine/events.hpp"

#include "cocaine/helpers/unique_id.hpp"

namespace cocaine { namespace engine {

namespace sc = boost::statechart;

// Slave states
// ------------

namespace slave {

struct unknown;
struct alive;
    struct idle;
    struct busy;
struct dead;

}

// Master FSM
// ----------

class master_t:
    public sc::state_machine<master_t, slave::unknown>,
    public unique_id_t
{
    friend struct slave::unknown;
    friend struct slave::alive;
    friend struct slave::busy;

    public:
        master_t(context_t& context,
                 engine_t& engine);

        ~master_t();
        
        bool operator==(const master_t& other) const;

        void unconsumed_event(const sc::event_base& event);

    private:
        void spawn();

        void on_initialize(const events::heartbeat& event);
        void on_heartbeat(const events::heartbeat& event);
        void on_terminate(const events::terminate& event);
        void on_timeout(ev::timer&, int);

    private:    
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        engine_t& m_engine;

        ev::timer m_heartbeat_timer;
        
        pid_t m_pid;
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
    void on_invoke(const events::invoke& event);
    void on_choke(const events::choke& event);

    ~alive();

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

}}

#endif
