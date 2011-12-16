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

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/slaves/process.hpp"

using namespace cocaine::engine::slave;

process_t::process_t(engine_t& engine, const std::string& type, const std::string& args):
    slave_t(engine)
{
    m_pid = fork();

    if(m_pid == 0) {
        // NOTE: Making a new context here to reinitialize the message bus
        context_t context(engine.context().config);

        overseer_t overseer(context, id(), m_engine.name());
        overseer(type, args);
        
        exit(EXIT_SUCCESS);
    } else if(m_pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    m_child_watcher.set<process_t, &process_t::signal>(this);
    m_child_watcher.start(m_pid);
}

void process_t::reap() {
    int status = 0;

    if(waitpid(m_pid, &status, WNOHANG) == 0) {
        ::kill(m_pid, SIGKILL);
    }

    // NOTE: There's no need to wait for the killed children,
    // as libev will automatically reap them.
    m_child_watcher.stop();
}

void process_t::signal(ev::child&, int) {
    if(!state_downcast<const dead*>()) {
        syslog(LOG_DEBUG, "%s: got a child termination signal", identity());
        process_event(events::terminated_t());
    }
}

