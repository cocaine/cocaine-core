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
}

void process_t::reap() {
    int status = 0;

    if(waitpid(m_pid, &status, WNOHANG) == 0) {
        // NOTE: There's no need to wait for the children,
        // as libev will automatically reap them.
        ::kill(m_pid, SIGKILL);
    }
}

