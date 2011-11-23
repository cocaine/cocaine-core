#include "cocaine/backends/process.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"

using namespace cocaine::engine::backends;

process_t::process_t(engine_t* engine, const std::string& type, const std::string& args):
    backend_t(engine)
{
    m_pid = fork();

    if(m_pid == 0) {
        zmq::context_t context(1);
        overseer_t overseer(id(), context, m_engine->name());
        overseer(type, args);
        exit(EXIT_SUCCESS);
    } else if(m_pid < 0) {
        throw resource_error_t("unable to spawn more workers");
    }

    m_child_watcher.set<process_t, &process_t::signal>(this);
    m_child_watcher.start(m_pid);
}

process_t::~process_t() {
    m_termination_timeout.stop();
    m_child_watcher.stop();
}

void process_t::stop() {
    m_termination_timeout.set<process_t, &process_t::terminate>(this);
    m_termination_timeout.start(5.0f);
}

void process_t::signal(ev::child&, int) {
    m_engine->reap(id());
}

void process_t::terminate(ev::timer&, int) {
    kill();
}

void process_t::kill() {
    ::kill(m_pid, SIGKILL);
}

