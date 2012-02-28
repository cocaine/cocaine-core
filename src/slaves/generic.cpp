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
        // NOTE: In order to reinitialize the subsystems in a new process
        engine.context().reset();

        overseer_t overseer(id(), engine.context(), engine.app());
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

    // TODO: Wait with a timeout?
    if(waitpid(m_pid, &status, WNOHANG) == 0) {
        ::kill(m_pid, SIGKILL);
    }

    // NOTE: There's no need to wait for the killed children,
    // as libev will automatically reap them.
    m_child_watcher.stop();
}

void generic_t::signal(ev::child&, int) {
    if(!state_downcast<const dead*>()) {
        log().debug("got a child termination signal");
        process_event(events::terminate_t());
    }
}

