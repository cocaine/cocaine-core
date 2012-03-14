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

#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cocaine/slave.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/engine.hpp"
#include "cocaine/job.hpp"
// #include "cocaine/rpc.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::slave;

// namespace {
//     struct this_slave {
//         this_slave(const slave_t& slave):
//             target(slave)
//         { }

//         bool operator()(pool_map_t::pointer slave) const {
//             return *slave->second == target;
//         }
    
//         const slave_t& target;
//     };
// }

slave_t::slave_t(engine_t& engine):
    m_engine(engine)
{
    // NOTE: These are the 10 seconds for the slave to come alive.
    m_heartbeat_timer.set<slave_t, &slave_t::timeout>(this);
    m_heartbeat_timer.start(10.0f);

    initiate();
    spawn();
}

slave_t::~slave_t() {
    m_heartbeat_timer.stop();
    
    // TEST: Make sure that the slave is really dead.
    BOOST_ASSERT(state_downcast<const dead*>() != 0);

    terminate();
}

void slave_t::react(const events::heartbeat_t& event) {
    if(!state_downcast<const alive*>()) {
#if EV_VERSION_MAJOR == 3 && EV_VERSION_MINOR == 8
        m_engine.app().log->debug(
            "slave %s came alive in %.03f seconds",
            id().c_str(),
            10.0f - ev_timer_remaining(
                ev_default_loop(ev::AUTO),
                static_cast<ev_timer*>(&m_heartbeat_timer)
            )
        );
#endif

        // rpc::packed<events::configure_t> pack(
        //     events::configure_t(m_engine.context().config)
        // );

        // m_engine.unicast(
        //     this_slave(*this),
        //     pack
        // );
    }

    m_heartbeat_timer.stop();
    
    const busy * state = state_downcast<const busy*>();
    float timeout = m_engine.app().policy.heartbeat_timeout;

    if(state && state->job()->policy().timeout > 0.0f) {
        timeout = state->job()->policy().timeout;
    }
           
    m_engine.app().log->debug(
        "resetting slave %s heartbeat timeout to %.02f seconds",
        id().c_str(),
        timeout
    );

    m_heartbeat_timer.start(timeout);

}

void slave_t::react(const events::terminate_t& event) {
    m_engine.app().log->debug(
        "reaping slave %s", 
        id().c_str()
    );

    int status = 0;

    // XXX: Is it needed at all? Might as well check the state.
    if(waitpid(m_pid, &status, WNOHANG) == 0) {
        ::kill(m_pid, SIGKILL);
    }

    // NOTE: Children are automatically reaped by libev.
    m_child_watcher.stop();
}

bool slave_t::operator==(const slave_t& other) const {
    return id() == other.id();
}

void slave_t::spawn() {
    m_pid = ::fork();

    if(m_pid == 0) {
#ifdef HAVE_CGROUPS
        if(m_engine.group()) {
            int rv = 0;
            
            if((rv = cgroup_attach_task(m_engine.group())) != 0) {
                m_engine.app().log->error(
                    "unable to attach slave %s to a control group - %s",
                    id().c_str(),
                    cgroup_strerror(rv)
                );

                exit(EXIT_FAILURE);
            }
        }
#endif

        int rv = 0;

        rv = ::execlp(
            m_engine.context().config.runtime.self.c_str(),
            m_engine.context().config.runtime.self.c_str(),
            "--slave",
            "--slave:id",       id().c_str(),
            "--slave:app",      m_engine.app().name.c_str(),
            "--core:modules",   m_engine.context().config.core.modules.c_str(),
            "--storage:driver", m_engine.context().config.storage.driver.c_str(),
            "--storage:uri",    m_engine.context().config.storage.uri.c_str(),
            (char*)0
        );

        if(rv != 0) {
            char message[1024];

            ::strerror_r(errno, message, 1024);

            m_engine.app().log->error(
                "unable to start slave %s: %s",
                id().c_str(),
                message
            );

            exit(EXIT_FAILURE);
        }
    } else if(m_pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    m_child_watcher.set<slave_t, &slave_t::signal>(this);
    m_child_watcher.start(m_pid);    
}

void slave_t::timeout(ev::timer&, int) {
    m_engine.app().log->warning(
        "slave %s missed too many heartbeats",
        id().c_str()
    );
    
    const busy * state = state_downcast<const busy*>();
    
    if(state) {
        state->job()->process_event(
            events::error_t(
                client::timeout_error, 
                "the job has timed out"
            )
        );
    }
    
    process_event(events::terminate_t());
}

void slave_t::signal(ev::child& event, int) {
    if(!state_downcast<const dead*>()) {
        process_event(events::terminate_t());
        
        if(WIFEXITED(event.rstatus) && WEXITSTATUS(event.rstatus) != EXIT_SUCCESS) {
            m_engine.app().log->warning(
                "slave %s terminated abnormally",
                id().c_str()
            );

            m_engine.stop();
        } else if(WIFSIGNALED(event.rstatus)) {
            m_engine.app().log->warning(
                "slave %s has been killed by %s", 
                id().c_str(), 
                strsignal(WTERMSIG(event.rstatus))
            );
            
            m_engine.stop();
        };
    }
}

alive::~alive() {
    if(m_job && !m_job->state_downcast<const job::complete*>()) {
        context<slave_t>().m_engine.app().log->debug(
            "rescheduling an incomplete '%s' job", 
            m_job->method().c_str()
        );
        
        context<slave_t>().m_engine.enqueue(m_job, true);
        m_job.reset();
    }
}

void alive::react(const events::invoke_t& event) {
    // TEST: Ensure that no job is being lost here.
    BOOST_ASSERT(!m_job);

    m_job = event.job;
    m_job->process_event(event);
    
    context<slave_t>().m_engine.app().log->debug(
        "job '%s' assigned to slave %s",
        m_job->method().c_str(),
        context<slave_t>().id().c_str()
    );
}

void alive::react(const events::release_t& event) {
    // TEST: Ensure that the job is in fact here.
    BOOST_ASSERT(m_job);

    context<slave_t>().m_engine.app().log->debug(
        "job '%s' completed by slave %s",
        m_job->method().c_str(),
        context<slave_t>().id().c_str()
    );
    
    m_job->process_event(event);
    m_job.reset();
}
