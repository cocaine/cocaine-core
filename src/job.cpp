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

#include <numeric>

#include "cocaine/job.hpp"
#include "cocaine/master.hpp"

using namespace cocaine::engine;

job_t::job_t(const std::string& event_):
    state(unknown),
    event(event_)
{ }

job_t::job_t(const std::string& event_, policy_t policy_):
    state(unknown),
    event(event_),
    policy(policy_)
{ }

job_t::~job_t() {
    // TEST: Only completed or new jobs can be destroyed.
    BOOST_ASSERT(state == complete || state == unknown);
}

void
job_t::push(const std::string& chunk) {
    lock_t lock(*this);
    
    if(state == complete) {
        throw std::runtime_error("job has been completed");
    }

    // NOTE: Put the new chunk into the cache (in case
    // the job will be retried or if the job is not yet
    // assigned to the slave). 
    cache.push_back(chunk);

    if(state == processing) {
        boost::shared_ptr<master_t> owner(
            master.lock()
        );

        // TEST: Orphan jobs is a bug.
        BOOST_ASSERT(owner);

        owner->push(chunk);
    }
}

void
job_t::process(const events::invoke& event) {
    lock_t lock(*this);

    // TEST: Jobs cannot be invoked when already completed.
    BOOST_ASSERT(state != complete);

    state = processing;
    master = event.master;

    boost::shared_ptr<master_t> owner(
        master.lock()
    );

    // TEST: Orphan jobs is a bug.
    BOOST_ASSERT(owner);

    for(chunk_list_t::const_iterator it = cache.begin();
        it != cache.end();
        ++it)
    {
        owner->push(*it);
    }
}

void
job_t::process(const events::chunk& event) {
    {
        lock_t lock(*this);

        // TEST: Jobs can only receive chunks when assigned to the slave.
        BOOST_ASSERT(state = processing);
    }

    react(event);
}

void
job_t::process(const events::error& event) {
    {
        lock_t lock(*this);

        if(state = complete) {
            // NOTE: Do not forward multiple errors. 
            return;
        }

        state = complete;
    }

    react(event);
}

void
job_t::process(const events::choke& event) {
    {
        lock_t lock(*this);

        // TODO: Is this necessary at all?
        master.reset();

        state = complete;
    }

    react(event);
}

namespace {
    struct accumulate_t {
        size_t
        operator()(size_t total, 
                   const std::string& chunk)
        {
            return total + chunk.size();
        }
    };
}

size_t
job_t::size() const {
    if(cache.empty()) {
        return 0;
    }

    return std::accumulate(
        cache.begin(),
        cache.end(),
        0,
        accumulate_t()
    );
}
