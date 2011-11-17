#include "cocaine/drivers/abstract.hpp"

using namespace boost::accumulators;
using namespace cocaine::engine;
        
driver_t::driver_t(engine_t* engine, const std::string& method):
    m_engine(engine),
    m_method(method),
    m_stats(tag::rolling_window::window_size = 10)
{
    syslog(LOG_DEBUG, "driver [%s:%s]: constructing", 
        m_engine->name().c_str(), m_method.c_str());
}

driver_t::~driver_t() {
    syslog(LOG_DEBUG, "driver [%s:%s]: destructing",
        m_engine->name().c_str(), m_method.c_str());
}

void driver_t::audit(ev::tstamp spent) {
    m_stats(spent);
}

void driver_t::expire(boost::shared_ptr<job_t> job) {
    m_engine->expire(job);
}

Json::Value driver_t::stats() const {
    Json::Value results(Json::objectValue);

    results["mean"] = rolling_mean(m_stats);
    results["events"] = static_cast<Json::UInt>(count(m_stats));
    results["spent"] = sum(m_stats);

    return results;
}

