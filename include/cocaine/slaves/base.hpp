#ifndef COCAINE_SLAVE_BASE_HPP
#define COCAINE_SLAVE_BASE_HPP

#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/transition.hpp>

#include "cocaine/common.hpp"
#include "cocaine/events.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine { namespace slave {

namespace sc = boost::statechart;

struct unknown;
struct alive;
    struct idle;
    struct busy;
struct dead;

struct slave_t:
    public sc::state_machine<slave_t, unknown>,
    public birth_control_t<slave_t>,
    public unique_id_t
{
    public:
        slave_t(engine_t* engine);
        virtual ~slave_t();

        void react(const events::heartbeat& event);
        
    public:
        virtual void reap() = 0;

    private:
        void timeout(ev::timer&, int);

    protected:
        engine_t* m_engine;

    private:
        ev::timer m_heartbeat_timer;
};

struct unknown:
    public sc::simple_state<unknown, slave_t> 
{
    public:
        typedef boost::mpl::list<
            sc::transition<events::heartbeat, alive, slave_t, &slave_t::react>,
            sc::transition<events::death, dead>
        > reactions;
};

struct alive:
    public sc::simple_state<alive, slave_t, idle>
{
    public:
        typedef boost::mpl::list<
            sc::in_state_reaction<events::heartbeat, slave_t, &slave_t::react>,
            sc::transition<events::death, dead>
        > reactions;

        ~alive();

        void react(const events::invoked& event);
        void react(const events::completed& event);

        const boost::shared_ptr<job::job_t>& job() const {
            return m_job;
        }

    private:
        boost::shared_ptr<job::job_t> m_job;
};

struct idle: 
    public sc::simple_state<idle, alive>
{
    public:
        typedef sc::transition<
            events::invoked, busy, alive, &alive::react
        > reactions;
};

struct busy:
    public sc::simple_state<busy, alive>
{
    public:
        typedef sc::transition<
            events::completed, idle, alive, &alive::react
        > reactions;

        const boost::shared_ptr<job::job_t>& job() const {
            return context<alive>().job();
        }
};

struct dead:
    public sc::state<dead, slave_t>
{
    public:
        dead(my_context ctx); 
};

}}}

#endif
