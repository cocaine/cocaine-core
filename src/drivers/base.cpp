#include "cocaine/drivers/base.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
        
driver_t::driver_t(engine_t* engine, const std::string& method):
    m_engine(engine),
    m_method(method)
{
    syslog(LOG_DEBUG, "driver [%s:%s]: constructing", 
        m_engine->name().c_str(), m_method.c_str());
}

driver_t::~driver_t() {
    syslog(LOG_DEBUG, "driver [%s:%s]: destructing",
        m_engine->name().c_str(), m_method.c_str());
}

void driver_t::audit(timing_type type, ev::tstamp timing) {
    switch(type) {
        case in_queue:
            m_spent_in_queues(timing);
            break;
        case on_slave:
            m_spent_on_slaves(timing);
            break;
    }
}

Json::Value driver_t::stats() const {
    Json::Value results(Json::objectValue);

    results["successful-jobs"] = static_cast<Json::UInt>(count(m_spent_on_slaves));
    results["spent-on-slaves"] = sum(m_spent_on_slaves);
    results["median-wait-time"] = median(m_spent_on_slaves);
    results["spent-in-queues"] = sum(m_spent_in_queues);
    results["median-processing-time"] = median(m_spent_in_queues);

    return results;
}

