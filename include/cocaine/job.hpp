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

#ifndef COCAINE_JOB_HPP
#define COCAINE_JOB_HPP

#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/transition.hpp>

#include "cocaine/common.hpp"

#include "cocaine/events.hpp"

#include "cocaine/helpers/blob.hpp"

#include "cocaine/dealer/types.hpp"

namespace cocaine { namespace engine {

namespace sc = boost::statechart;

// Job states
// ----------

namespace job {

struct incomplete;
    struct unknown;
    struct waiting;
    struct processing;
struct complete;

}

// Job FSM
// -------

class job_t:
    public sc::state_machine<job_t, job::incomplete>,
    public birth_control_t<job_t>
{
    friend struct job::incomplete;
    friend struct job::waiting;
    friend struct job::processing;

    public:
        job_t(drivers::driver_t& driver);
        job_t(drivers::driver_t& driver, client::policy_t policy);
        job_t(drivers::driver_t& driver, const blob_t& request);
        
        job_t(drivers::driver_t& driver,
              client::policy_t policy, 
              const blob_t& request);

        virtual ~job_t();

    protected:
        virtual void react(const events::push_t& event);
        virtual void react(const events::error_t& event);
        virtual void react(const events::release_t& event);

    public:
        const std::string& method() const {
            return m_method;
        }
        
        const client::policy_t& policy() const {
            return m_policy;
        }

        const blob_t& request() const {
            return m_request;
        }

    private:
        void discard(ev::periodic&, int);

    protected:
        drivers::driver_t& m_driver;

    private:
        const std::string m_method;
        const client::policy_t m_policy;
        blob_t m_request;

        ev::periodic m_expiration_timer;
};

namespace job {

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
            sc::transition<events::enqueue_t, waiting>,
            sc::transition<events::invoke_t, processing>
        > reactions;
};

struct waiting:
    public sc::simple_state<waiting, incomplete>
{
    public:
        typedef sc::transition<
            events::invoke_t, processing
        > reactions;

        waiting();
        ~waiting();

    private:
        const ev::tstamp m_timestamp;
};

struct processing:
    public sc::simple_state<processing, incomplete>
{
    public:
        typedef boost::mpl::list<
            sc::in_state_reaction<events::push_t, job_t, &job_t::react>,
            sc::transition<events::release_t, complete, job_t, &job_t::react>,
            sc::transition<events::enqueue_t, waiting>,
            sc::transition<events::invoke_t, processing>
        > reactions;

        processing();
        ~processing();

    private:
        const ev::tstamp m_timestamp;
};

struct complete:
    public sc::simple_state<complete, job_t>
{ };

}

}}

#endif
