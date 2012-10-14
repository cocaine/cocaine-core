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

#include "cocaine/engine.hpp"
#include "cocaine/master.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/traits/unique_id.hpp"

using namespace cocaine::engine;

job_t::job_t(const std::string& event_):
    state(state::unknown),
    event(event_)
{ }

job_t::job_t(const std::string& event_, policy_t policy_):
    state(state::unknown),
    event(event_),
    policy(policy_)
{ }

job_t::~job_t() {
    // TEST: Active jobs cannot be destroyed.
    BOOST_ASSERT(state != state::processing);
}

void
job_t::process(const events::invoke& event) {
    boost::unique_lock<boost::mutex> lock(mutex);    

    // TEST: Jobs cannot be invoked when already completed.
    BOOST_ASSERT(state != state::complete);

    state = state::processing;
    engine = event.engine;
    master = event.master;

    if(!cache.empty()) {
        boost::shared_ptr<master_t> owner(master.lock());

        // TEST: There cannot be any orphan jobs.
        BOOST_ASSERT(engine && owner);

        for(chunk_list_t::const_iterator it = cache.begin();
            it != cache.end();
            ++it)
        {
            send(owner->id(), *it);
        }
    }
}

void
job_t::process(const events::chunk& event) {
    {
        boost::unique_lock<boost::mutex> lock(mutex);    
        
        // TEST: Jobs cannot process chunks when not active.
        BOOST_ASSERT(state == state::processing);
    }

    react(event);
}

void
job_t::process(const events::error& event) {
    {
        boost::unique_lock<boost::mutex> lock(mutex);    
        
        state = state::complete;
        cache.clear();
    }

    react(event);
}

void
job_t::process(const events::choke& event) {
    {
        boost::unique_lock<boost::mutex> lock(mutex);    
        
        state = state::complete;
        cache.clear();
    }
    
    react(event);
}

void
job_t::push(const std::string& data) {
    boost::unique_lock<boost::mutex> lock(mutex);

    if(state == state::complete) {
        throw std::runtime_error("job has been already completed");
    }

    // NOTE: Put the new chunk into the cache (in case
    // the job will be retried or if the job is not yet
    // assigned to the slave). 
    cache.emplace_back(data);

    if(state == state::processing) {
        boost::shared_ptr<master_t> owner(
            master.lock()
        );
   
        // TEST: There cannot be any orphan jobs.
        BOOST_ASSERT(engine && owner);

        send(owner->id(), data);
    }
}

void
job_t::send(const unique_id_t& uuid,
            const std::string& data)
{
    zmq::message_t chunk(data.size());

    memcpy(
        chunk.data(),
        data.data(),
        data.size()
    );

    io::message<rpc::chunk> message(chunk);

    engine->send(
        uuid,
        message
    );
}

