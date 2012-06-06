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
#include "cocaine/helpers/birth_control.hpp"

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

struct job_t:
    public sc::state_machine<job_t, job::incomplete>,
    public birth_control_t<job_t>
{
    job_t(const std::string& event);

    job_t(const std::string& event,
          const blob_t& request);

    job_t(const std::string& event,
          dealer::policy_t policy);
    
    job_t(const std::string& event,
          const blob_t& request,
          dealer::policy_t policy); 

    virtual ~job_t();

    virtual void react(const events::chunk& event);
    virtual void react(const events::error& event);
    virtual void react(const events::choke& event);

    const std::string event;
    const dealer::policy_t policy;
    const blob_t request;
};

namespace job {

struct incomplete:
    public sc::simple_state<incomplete, job_t, unknown>
{
    typedef boost::mpl::list<
        sc::transition<events::error, complete, job_t, &job_t::react>
    > reactions;
};

struct unknown:
    public sc::simple_state<unknown, incomplete>
{
    typedef boost::mpl::list<
        sc::transition<events::enqueue, waiting>,
        sc::transition<events::invoke, processing>
    > reactions;
};

struct waiting:
    public sc::simple_state<waiting, incomplete>
{
    typedef boost::mpl::list<
        sc::transition<events::invoke, processing>
    > reactions;

    waiting();
    ~waiting();

    const double timestamp;
};

struct processing:
    public sc::simple_state<processing, incomplete>
{
    typedef boost::mpl::list<
        sc::in_state_reaction<events::chunk, job_t, &job_t::react>,
        sc::transition<events::choke, complete, job_t, &job_t::react>,
        sc::transition<events::enqueue, waiting>,
        sc::transition<events::invoke, processing>
    > reactions;

    processing();
    ~processing();

    const double timestamp;
};

struct complete:
    public sc::simple_state<complete, job_t>
{ };

}

}}

#endif
