#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/slaves/thread.hpp"

using namespace cocaine::engine::slave;

thread_t::thread_t(engine_t* engine, const std::string& type, const std::string& args):
    slave_t(engine)
{
    try {
        m_overseer.reset(new overseer_t(id(), m_engine->context(), m_engine->name()));
        m_thread.reset(new boost::thread(boost::ref(*m_overseer), type, args));
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("clone() failed");
    }
}

void thread_t::reap() {
    if(!m_thread->timed_join(boost::posix_time::seconds(5))) {
        m_thread->interrupt();
        m_thread.reset();
    }
}

