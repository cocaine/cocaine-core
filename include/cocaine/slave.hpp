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

#ifndef COCAINE_SLAVE_OVERSEER_HPP
#define COCAINE_SLAVE_OVERSEER_HPP

#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/transition.hpp>

#include "cocaine/common.hpp"

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

// Slave FSM
// ---------

class slave_t:
    public sc::state_machine<slave_t, slave::unknown>,
    public unique_id_t
{
    friend struct slave::unknown;
    friend struct slave::alive;

    public:
        slave_t(engine_t& engine);
        ~slave_t();
        
        bool operator==(const slave_t& other) const;

    private:
        void spawn();

        void on_configure(const events::heartbeat_t& event);
        void on_heartbeat(const events::heartbeat_t& event);
        void on_terminate(const events::terminate_t& event);

        void on_timeout(ev::timer&, int);
        void on_signal(ev::child& event, int);

    private:    
        engine_t& m_engine;
        
        pid_t m_pid;

        ev::timer m_heartbeat_timer;
        ev::child m_child_watcher;
};

namespace slave {

struct unknown:
    public sc::simple_state<unknown, slave_t> 
{
    public:
        typedef boost::mpl::list<
            sc::transition<events::heartbeat_t, alive, slave_t, &slave_t::on_configure>,
            sc::transition<events::terminate_t, dead,  slave_t, &slave_t::on_terminate>
        > reactions;
};

struct alive:
    public sc::simple_state<alive, slave_t, idle>
{
    public:
        typedef boost::mpl::list<
            sc::in_state_reaction<events::heartbeat_t, slave_t, &slave_t::on_heartbeat>,
            sc::transition<events::terminate_t, dead,  slave_t, &slave_t::on_terminate>
        > reactions;

        ~alive();

        void on_invoke(const events::invoke_t& event);
        void on_release(const events::release_t& event);

    public:
        const std::auto_ptr<job_t>& job() const {
            BOOST_ASSERT(m_job.get());
            return m_job;
        }

    private:
        std::auto_ptr<job_t> m_job;
};

struct idle: 
    public sc::simple_state<idle, alive>
{
    public:
        typedef sc::transition<
            events::invoke_t, busy, alive, &alive::on_invoke
        > reactions;
};

struct busy:
    public sc::simple_state<busy, alive>
{
    public:
        typedef sc::transition<
            events::release_t, idle, alive, &alive::on_release
        > reactions;

    public:
        const std::auto_ptr<job_t>& job() const {
            return context<alive>().job();
        }
};

struct dead:
    public sc::simple_state<dead, slave_t>
{ };

}

}}

#endif
