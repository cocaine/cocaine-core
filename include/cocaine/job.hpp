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

#ifndef COCAINE_JOB_HPP
#define COCAINE_JOB_HPP

#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/transition.hpp>

#include "cocaine/common.hpp"

#include "cocaine/events.hpp"
#include "cocaine/policy.hpp"

#include "cocaine/helpers/blob.hpp"
#include "cocaine/helpers/birth_control.hpp"

namespace cocaine { namespace engine {

namespace sc = boost::statechart;

// Job states
// ----------

namespace job {

struct incomplete;
    struct unknown;
    struct processing;
struct complete;

}

// Job FSM
// -------

struct job_t:
    public sc::state_machine<job_t, job::incomplete>,
    public birth_control<job_t>
{
    job_t(const std::string& event);

    job_t(const std::string& event,
          const blob_t& request);

    job_t(const std::string& event,
          policy_t policy);
    
    job_t(const std::string& event,
          const blob_t& request,
          policy_t policy); 

    virtual ~job_t();

    virtual void react(const events::chunk& ) { }
    virtual void react(const events::error& ) { }
    virtual void react(const events::choke& ) { }

    const std::string event;
    const blob_t request;
    const policy_t policy;
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
        sc::transition<events::invoke, processing>
    > reactions;
};

struct processing:
    public sc::simple_state<processing, incomplete>
{
    typedef boost::mpl::list<
        sc::in_state_reaction<events::chunk, job_t, &job_t::react>,
        sc::transition<events::choke, complete, job_t, &job_t::react>,
        sc::transition<events::invoke, processing>
    > reactions;
};

struct complete:
    public sc::simple_state<complete, job_t>
{ };

}

}}

#endif
