#include "cocaine/backends/thread.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::backends;

thread_t::thread_t(engine_t* engine, const std::string& type, const std::string& args):
    backend_t(engine)
{
    try {
        m_overseer.reset(new overseer_t(id(), m_engine->context(), m_engine->name()));
        m_thread.reset(new boost::thread(boost::ref(*m_overseer), type, args));
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("unable to spawn more workers");
    }
}

thread_t::~thread_t() {
    if(m_thread.get()) {
        if(!m_thread->timed_join(boost::posix_time::seconds(5))) {
            syslog(LOG_WARNING, "worker [%s:%s]: worker is unresponsive",
                m_engine->name().c_str(), id().c_str());
            m_thread->interrupt();
        }
    }
}

void thread_t::kill() {
    m_thread->interrupt();
    m_thread.reset();
}

