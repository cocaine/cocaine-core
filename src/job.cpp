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
    // TEST: Ensure that the job has been completed.
    BOOST_ASSERT(state_downcast<const complete*>() != 0);

    terminate();
}
