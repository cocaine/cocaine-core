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

#include "cocaine/session.hpp"

#include "cocaine/engine.hpp"
#include "cocaine/master.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/api/job.hpp"

#include "cocaine/traits/unique_id.hpp"

using namespace cocaine::engine;

session_t::session_t(const boost::shared_ptr<job_t>& job_):
    state(state::unknown),
    job(job_)
{ }

session_t::~session_t() {
    // TEST: Active jobs cannot be destroyed.
    BOOST_ASSERT(state != state::processing);
}

void
session_t::process(const events::invoke& event) {
    // TEST: Jobs cannot be invoked when already completed.
    BOOST_ASSERT(state != state::complete);
    
    state = state::processing;

    // The controlling master.
    m_master = event.master;
    
    boost::unique_lock<boost::mutex> lock(m_mutex);    

    if(!m_cache.empty()) {
        boost::shared_ptr<master_t> owner(m_master.lock());

        // TEST: There cannot be any orphan jobs.
        BOOST_ASSERT(owner);

        for(chunk_list_t::const_iterator it = m_cache.begin();
            it != m_cache.end();
            ++it)
        {
            owner->push(*it);
        }
    }
}

void
session_t::process(const events::chunk& event) {
    // TEST: Sessions cannot process chunks when not active.
    BOOST_ASSERT(state == state::processing);

    job->react(event);
}

void
session_t::process(const events::error& event) {
    // TEST: Sessions cannot be failed more than once.
    BOOST_ASSERT(state != state::complete);
    
    state = state::complete;
    
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);    
        m_cache.clear();
    }

    job->react(event);
}

void
session_t::process(const events::choke& event) {
    // TEST: Sessions cannot be completed more than once.
    BOOST_ASSERT(state != state::complete);

    state = state::complete;
    
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);    
        m_cache.clear();
    }
    
    job->react(event);
}

void
session_t::push(const std::string& data) {
    if(state == state::complete) {
        throw error_t("job has been already completed");
    }

    {
        boost::unique_lock<boost::mutex> lock(m_mutex);

        // NOTE: Put the new chunk into the cache (in case
        // the job will be retried or if the job is not yet
        // assigned to the slave). 
        m_cache.emplace_back(data);
    }

    if(state == state::processing) {
        boost::shared_ptr<master_t> owner(m_master.lock());
   
        // TEST: There cannot be any orphan jobs.
        BOOST_ASSERT(owner);

        owner->push(data);
    }
}
