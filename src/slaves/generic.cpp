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

#include <sys/wait.h>

#include "cocaine/slaves/generic.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"

using namespace cocaine::engine::slaves;

generic_t::generic_t(engine_t& engine):
    slave_t(engine)
{
    m_pid = fork();

    if(m_pid == 0) {
        context_t context(engine.context());

        if(context.config.core.cgroups) {
            int rv = 0;
            
            if((rv = cgroup_attach_task(engine.group())) != 0) {
                log().error(
                    "unable to attach to a control group - %s",
                    cgroup_strerror(rv)
                );

                exit(EXIT_FAILURE);
            }
        }

        overseer_t overseer(id(), context, engine.app());
        overseer.loop();
        
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
        log().debug("got a child termination signal");
        
        process_event(events::terminate_t());
        
        if(WIFEXITED(event.rstatus) && WEXITSTATUS(event.rstatus) == EXIT_FAILURE) {
            log().warning("unable to start");
            m_engine.stop();
        } else if(WIFSIGNALED(event.rstatus)) {
            log().warning("killed by a %d signal", WTERMSIG(event.rstatus));
            m_engine.stop();
        } else {
            log().warning("terminated in a strange way");
        }
    }
}

