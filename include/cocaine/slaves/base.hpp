//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_SLAVE_FRONTEND_BASE_HPP
#define COCAINE_SLAVE_FRONTEND_BASE_HPP

#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/transition.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/object.hpp"

#include "cocaine/events.hpp"

namespace cocaine { namespace engine { namespace slaves {

namespace sc = boost::statechart;

struct unknown;
struct alive;
    struct idle;
    struct busy;
struct dead;

struct slave_t:
    public sc::state_machine<slave_t, unknown>,
    public birth_control_t<slave_t>,
    public unique_id_t,
    public object_t
{
    friend class alive;

    public:
        virtual ~slave_t();
        
        void react(const events::heartbeat_t& event);
        void react(const events::terminate_t& event);

        bool operator==(const slave_t& other) const;

        virtual void reap() = 0;

    protected:
        slave_t(engine_t& engine);

    private:
        void timeout(ev::timer&, int);

    private:
        engine_t& m_engine;
        ev::timer m_heartbeat_timer;
};

struct unknown:
    public sc::simple_state<unknown, slave_t> 
{
    public:
        typedef boost::mpl::list<
            sc::transition<events::heartbeat_t, alive, slave_t, &slave_t::react>,
            sc::transition<events::terminate_t, dead,  slave_t, &slave_t::react>
        > reactions;
};

struct alive:
    public sc::simple_state<alive, slave_t, idle>
{
    public:
        typedef boost::mpl::list<
            sc::in_state_reaction<events::heartbeat_t, slave_t, &slave_t::react>,
            sc::transition<events::terminate_t, dead,  slave_t, &slave_t::react>
        > reactions;

        ~alive();

        void react(const events::invoke_t& event);
        void react(const events::release_t& event);

    public:
        const boost::shared_ptr<job_t>& job() const {
            return m_job;
        }

    private:
        boost::shared_ptr<job_t> m_job;
};

struct idle: 
    public sc::simple_state<idle, alive>
{
    public:
        typedef sc::transition<
            events::invoke_t, busy, alive, &alive::react
        > reactions;
};

struct busy:
    public sc::simple_state<busy, alive>
{
    public:
        typedef sc::transition<
            events::release_t, idle, alive, &alive::react
        > reactions;

    public:
        const boost::shared_ptr<job_t>& job() const {
            return context<alive>().job();
        }
};

struct dead:
    public sc::simple_state<dead, slave_t>
{ };

}}}

#endif
