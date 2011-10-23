#include "cocaine/backends/abstract.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine;

backend_t::backend_t(boost::shared_ptr<engine_t> parent):
    m_parent(parent)
{
    m_heartbeat.set<backend_t, &backend_t::timeout>(this);
    
    // First heartbeat is only to ensure that the worker has started
    rearm(10.);
}

backend_t::~backend_t() {
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }
}

void backend_t::rearm(float timeout) {
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }

    m_heartbeat.start(timeout);
}

backend_t::request_queue_t& backend_t::queue() {
    return m_queue;
}

const backend_t::request_queue_t& backend_t::queue() const {
    return m_queue;
}
