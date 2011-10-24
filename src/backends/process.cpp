#include "cocaine/backends/process.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"

using namespace cocaine::engine;
using namespace cocaine::plugin;

process_t::process_t(boost::shared_ptr<engine_t> parent, boost::shared_ptr<source_t> source):
    m_parent(parent)
{
    syslog(LOG_DEBUG, "worker [%s:%s]: constructing", m_parent->name().c_str(), id().c_str());

    m_pid = fork();

    if(m_pid == 0) {
        zmq::context_t context(1);
        overseer_t overseer(id(), context, m_parent->name());

#if BOOST_VERSION >= 103500
        overseer(source);
#else
        overseer.run(source);
#endif

        exit(EXIT_SUCCESS);
    } else if(m_pid < 0) {
        throw std::runtime_error("unable to fork");
    }
}

process_t::~process_t() {
    syslog(LOG_DEBUG, "worker [%s:%s]: destructing", m_parent->name().c_str(), id().c_str());
}

void process_t::timeout(ev::timer& w, int revents) {
    syslog(LOG_ERR, "worker [%s:%s]: worker missed a heartbeat",
        m_parent->name().c_str(), id().c_str());
    kill(m_pid, SIGKILL);
}
