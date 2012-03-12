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

#include <sys/wait.h>

#include "cocaine/slaves/generic.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/overseer.hpp"

using namespace cocaine::engine::slaves;

generic_t::generic_t(engine_t& engine):
    slave_t(engine)
{
    m_pid = fork();

    if(m_pid == 0) {
        context_t context(engine.context());

#ifdef HAVE_CGROUPS
        if(context.config.core.cgroups && engine.group()) {
            int rv = 0;
            
            if((rv = cgroup_attach_task(engine.group())) != 0) {
                m_engine.app().log->error(
                    "unable to attach slave %s to a control group - %s",
                    id().c_str(),
                    cgroup_strerror(rv)
                );

                exit(EXIT_FAILURE);
            }
        }
#endif

        overseer_t overseer(id(), context, engine.app());
        overseer.run();
        
        exit(EXIT_SUCCESS);
    } else if(m_pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    m_child_watcher.set<generic_t, &generic_t::signal>(this);
    m_child_watcher.start(m_pid);
}

void generic_t::reap() {
    int status = 0;

    // XXX: Is it needed at all? Might as well check the state.
    if(waitpid(m_pid, &status, WNOHANG) == 0) {
        ::kill(m_pid, SIGKILL);
    }

    // NOTE: Children are automatically reaped by libev.
    m_child_watcher.stop();
}

void generic_t::signal(ev::child& event, int) {
    if(!state_downcast<const dead*>()) {
        process_event(events::terminate_t());
        
        if(WIFEXITED(event.rstatus) && WEXITSTATUS(event.rstatus) == EXIT_FAILURE) {
            m_engine.app().log->warning(
                "slave %s failed to start",
                id().c_str()
            );

            m_engine.stop();
        } else if(WIFSIGNALED(event.rstatus)) {
            m_engine.app().log->warning(
                "slave %s has been killed by a signal %d", 
                id().c_str(), 
                WTERMSIG(event.rstatus)
            );
            
            m_engine.stop();
        };
    }
}

