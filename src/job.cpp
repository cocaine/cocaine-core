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

#include "cocaine/job.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::job;

job_t::job_t(const std::string& event_):
    event(event_)
{
    initiate();
}

job_t::job_t(const std::string& event_, const blob_t& request_):
    event(event_),
    request(request_)
{
    initiate();
}

job_t::job_t(const std::string& event_, policy_t policy_):
    event(event_),
    policy(policy_)
{
    initiate();
}

job_t::job_t(const std::string& event_, const blob_t& request_, policy_t policy_):
    event(event_),
    request(request_),
    policy(policy_)
{
    initiate();
}

job_t::~job_t() {
    // TEST: Ensure that the job has been completed or not started at all.
    BOOST_ASSERT(state_downcast<const complete*>() != 0
                 || state_downcast<const unknown*>() != 0); 

    terminate();
}
