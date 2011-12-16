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

#ifndef COCAINE_JOB_HPP
#define COCAINE_JOB_HPP

#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/transition.hpp>

#include "cocaine/client/types.hpp"
#include "cocaine/common.hpp"
#include "cocaine/events.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace engine { namespace job {

namespace sc = boost::statechart;

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
        job_t(driver::driver_t& driver, const client::policy_t& policy);
        virtual ~job_t();

    public:      
        virtual void react(const events::chunk_t& event) = 0;
        virtual void react(const events::error_t& event) = 0;
        virtual void react(const events::choked_t& event) { }

    public:
        inline driver::driver_t& driver() {
            return m_driver;
        }

        inline const client::policy_t& policy() const {
            return m_policy;
        }

        inline zmq::message_t* request() {
            return &m_request; 
        }

    private:
        void discard(ev::periodic&, int);

    protected:
        driver::driver_t& m_driver;

    private:
        client::policy_t m_policy;
        ev::periodic m_expiration_timer;
        zmq::message_t m_request;
};

struct incomplete:
    public sc::simple_state<incomplete, job_t, unknown>
{
    public:
        typedef boost::mpl::list<
            sc::transition<events::error_t, complete, job_t, &job_t::react>
        > reactions;
};

struct unknown:
    public sc::simple_state<unknown, incomplete>
{
    public:
        typedef boost::mpl::list<
            sc::transition<events::enqueued_t, waiting>,
            sc::transition<events::invoked_t, processing>
        > reactions;
};

struct waiting:
    public sc::simple_state<waiting, incomplete>
{
    public:
        typedef sc::transition<
            events::invoked_t, processing
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
            sc::in_state_reaction<events::chunk_t, job_t, &job_t::react>,
            sc::transition<events::choked_t, complete, job_t, &job_t::react>,
            sc::transition<events::enqueued_t, waiting>,
            sc::transition<events::invoked_t, processing>
        > reactions;

        processing();
        ~processing();

    private:
        ev::tstamp m_timestamp;
};

struct complete:
    public sc::simple_state<complete, job_t>
{ };

}}}

#endif
