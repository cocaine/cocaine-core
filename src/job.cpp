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

#include "cocaine/drivers/base.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/job.hpp"

using namespace cocaine::engine::job;

job_t::job_t(driver::driver_t& driver, client::policy_t policy):
    m_driver(driver),
    m_policy(policy)
{
    if(m_policy.deadline) {
        m_expiration_timer.set<job_t, &job_t::discard>(this);
        m_expiration_timer.start(m_policy.deadline);
    }

    initiate();
}

job_t::~job_t() {
    m_expiration_timer.stop();

    // TEST: Ensure that the job has been completed
    BOOST_ASSERT(state_downcast<const complete*>() != 0);

    terminate();
}

void job_t::discard(ev::periodic&, int) {
    process_event(
        events::error_t(
            client::deadline_error,
            "the job has expired"
        )
    );
}

waiting::waiting():
    m_timestamp(ev::get_default_loop().now())
{ }

waiting::~waiting() {
    context<job_t>().driver().audit(
        driver::in_queue,
        ev::get_default_loop().now() - m_timestamp
    );
}

processing::processing():
    m_timestamp(ev::get_default_loop().now())
{ }

processing::~processing() {
    context<job_t>().driver().audit(
        driver::on_slave,
        ev::get_default_loop().now() - m_timestamp
    );
}

