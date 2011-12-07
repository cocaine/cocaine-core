#include <sys/wait.h>

#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/slaves/process.hpp"

using namespace cocaine::engine::slave;

process_t::process_t(engine_t* engine, const std::string& type, const std::string& args):
    slave_t(engine)
{
    m_pid = fork();

    if(m_pid == 0) {
        zmq::context_t context(1);
        overseer_t overseer(id(), context, m_engine->name());
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
    syslog(LOG_DEBUG, "slave [%s:%s]: got a child termination signal", 
        m_engine->name().c_str(), id().c_str());
    process_event(events::terminated_t());
}

