#ifndef COCAINE_JOB_HPP
#define COCAINE_JOB_HPP

#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/transition.hpp>

#include "cocaine/common.hpp"
#include "cocaine/events.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine { namespace job {

namespace sc = boost::statechart;

struct policy_t {
    public:
        policy_t();
        policy_t(bool urgent, ev::tstamp timeout, ev::tstamp deadline);

    public:
        bool urgent;
        ev::tstamp timeout;
        ev::tstamp deadline;
};

// Job states
struct incomplete;
    struct unknown;
    struct waiting;
    struct processing;
struct complete;

// Job FSM
class job_t:
    public sc::state_machine<job_t, incomplete>,
    public birth_control_t<job_t>
{
    public:
        job_t(driver::driver_t* driver, policy_t policy);
        virtual ~job_t();

    public:      
        virtual void react(const events::response& event) = 0;
        virtual void react(const events::error& event) = 0;

    public:
        virtual void react(const events::request_error& event) {
            react(static_cast<const events::error&>(event));
        }
        
        virtual void react(const events::resource_error& event) {
            react(static_cast<const events::error&>(event));
        }

        virtual void react(const events::server_error& event) {
            react(static_cast<const events::error&>(event));
        }

        virtual void react(const events::timeout_error& event) {
            react(static_cast<const events::error&>(event));
        }

        virtual void react(const events::application_error& event) {
            react(static_cast<const events::error&>(event));
        }

        virtual void react(const events::exemption& event) { }
    
    public:
        inline driver::driver_t* driver() {
            return m_driver;
        }

        inline policy_t policy() const {
            return m_policy;
        }

        inline zmq::message_t* request() {
            return &m_request; 
        }

    private:
        void discard(ev::periodic&, int);

    protected:
        driver::driver_t* m_driver;

    private:
        policy_t m_policy;
        ev::periodic m_expiration_timer;
        zmq::message_t m_request;
};

struct incomplete:
    public sc::simple_state<incomplete, job_t, unknown>
{
    public:
        typedef boost::mpl::list<
            sc::transition<events::timeout_error, complete, job_t, &job_t::react>, 
            sc::transition<events::server_error,  complete, job_t, &job_t::react>
        > reactions;
};

struct unknown:
    public sc::simple_state<unknown, incomplete>
{
    public:
        typedef boost::mpl::list<
            sc::transition<events::request_error,  complete, job_t, &job_t::react>,
            sc::transition<events::resource_error, complete, job_t, &job_t::react>,
            sc::transition<events::queuing,        waiting>,
            sc::transition<events::assignment,     processing>
        > reactions;
};

struct waiting:
    public sc::simple_state<waiting, incomplete>
{
    public:
        typedef sc::transition<
            events::assignment, processing
        > reactions;

        waiting();
        ~waiting();

    private:
        ev::tstamp m_timestamp;
};

struct processing:
    public sc::simple_state<processing, incomplete>
{
    public:
        typedef boost::mpl::list<
            sc::in_state_reaction<events::response,          job_t, &job_t::react>,
            sc::in_state_reaction<events::application_error, job_t, &job_t::react>,
            sc::transition<events::exemption, complete,      job_t, &job_t::react>
        > reactions;

        processing();
        ~processing();

    private:
        ev::tstamp m_timestamp;
};

struct complete:
    public sc::simple_state<complete, job_t>
{
};

}}}

#endif
