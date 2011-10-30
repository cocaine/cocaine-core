#include "cocaine/backends/process.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::backends;

process_t::process_t(boost::shared_ptr<engine_t> engine, const std::string& type, const std::string& args):
    m_engine(engine)
{
    syslog(LOG_DEBUG, "worker [%s:%s]: constructing", m_engine->name().c_str(), id().c_str());

    m_pid = fork();

    if(m_pid == 0) {
        zmq::context_t context(1);
        overseer_t overseer(id(), context, m_engine->name());

        overseer(type, args);

        exit(EXIT_SUCCESS);
    } else if(m_pid < 0) {
        throw std::runtime_error("unable to spawn more workers");
    }

    m_child_watcher.set<process_t, &process_t::signal>(this);
    m_child_watcher.start(m_pid);
}

process_t::~process_t() {
    syslog(LOG_DEBUG, "worker [%s:%s]: destructing", m_engine->name().c_str(), id().c_str());
}

void process_t::timeout(ev::timer& w, int revents) {
    syslog(LOG_ERR, "worker [%s:%s]: worker has missed too many heartbeats",
        m_engine->name().c_str(), id().c_str());
    kill(m_pid, SIGKILL);
}

void process_t::signal(ev::child& w, int revents) {
    m_engine->reap(id());
}
