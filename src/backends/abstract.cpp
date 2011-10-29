#include "cocaine/backends/abstract.hpp"

using namespace cocaine::engine::backends;

backend_t::backend_t():
    m_active(false)
{
    m_heartbeat.set<backend_t, &backend_t::timeout>(this);
    m_heartbeat.start(10.);
}

backend_t::~backend_t() {
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }
}

bool backend_t::active() const {
    return m_active;
}

void backend_t::rearm(float timeout) {
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }

    m_heartbeat.start(timeout);

    m_active = true;
}

backend_t::deferred_queue_t& backend_t::queue() {
    return m_queue;
}

const backend_t::deferred_queue_t& backend_t::queue() const {
    return m_queue;
}
