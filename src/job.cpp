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

#include "cocaine/drivers/base.hpp"

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::client;

job_t::job_t(drivers::driver_t& driver):
    m_driver(driver)
{
    initiate();
}

job_t::job_t(drivers::driver_t& driver, policy_t policy):
    m_driver(driver),
    m_policy(policy)
{
    if(m_policy.deadline) {
        m_expiration_timer.set<job_t, &job_t::discard>(this);
        m_expiration_timer.start(m_policy.deadline);
    }

    initiate();
}

job_t::job_t(drivers::driver_t& driver, const blob_t& request):
    m_driver(driver),
    m_request(request)
{
    initiate();
}

job_t::job_t(drivers::driver_t& driver, policy_t policy, const blob_t& request):
    m_driver(driver),
    m_policy(policy),
    m_request(request)
{
    if(m_policy.deadline) {
        m_expiration_timer.set<job_t, &job_t::discard>(this);
        m_expiration_timer.start(m_policy.deadline);
    }

    initiate();
}

job_t::~job_t() {
    m_expiration_timer.stop();

    // TEST: Ensure that the job has been completed.
    BOOST_ASSERT(state_downcast<const complete*>() != 0);

    terminate();
}

void job_t::react(const events::push_t& event) {
    // TODO: Emitters.
}

void job_t::react(const events::error_t& event) {
    // TODO: Emitters.
}

void job_t::react(const events::release_t& event) {
    // TODO: Emitters.
}

const std::string& job_t::method() const {
    return m_driver.method();
}

// XXX: Might be a good idea to stop this timer when the job is invoked.
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
    context<job_t>().m_driver.audit(
        drivers::in_queue,
        ev::get_default_loop().now() - m_timestamp
    );
}

processing::processing():
    m_timestamp(ev::get_default_loop().now())
{ }

processing::~processing() {
    context<job_t>().m_driver.audit(
        drivers::on_slave,
        ev::get_default_loop().now() - m_timestamp
    );
}

